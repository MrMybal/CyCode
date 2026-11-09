"""Prepare a local review dossier. Never execute, copy, upload or rebuild a sample."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path, PureWindowsPath
import xml.etree.ElementTree as ET
import re
import struct

SUPPORT='https://support.kaspersky.com/common/error/other/1870'
ALLOWLIST='https://www.kaspersky.com/partners/allowlist-program'

def sha256_file(path):
    digest=hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda:stream.read(1024*1024),b''):digest.update(block)
    return digest.hexdigest()

def pe_metadata(path):
    """Read data bytes only; an embedded certificate is not a trust verdict."""
    with path.open('rb') as stream:
        stream.seek(0,2);size=stream.tell();stream.seek(0)
        dos=stream.read(64)
        if len(dos)!=64 or dos[:2]!=b'MZ':return {'format':'unknown'}
        offset=struct.unpack_from('<I',dos,60)[0]
        if offset<64 or offset>size-24:return {'format':'invalid-pe'}
        stream.seek(offset);header=stream.read(24)
        if header[:4]!=b'PE\0\0':return {'format':'invalid-pe'}
        machine=struct.unpack_from('<H',header,4)[0]
        optional_size=struct.unpack_from('<H',header,20)[0]
        if optional_size<72 or offset+24+optional_size>size:return {'format':'invalid-pe'}
        optional=stream.read(optional_size);magic=struct.unpack_from('<H',optional)[0]
        if magic not in (0x10b,0x20b):return {'format':'invalid-pe'}
        directory_offset=96 if magic==0x10b else 112
        certificate=False
        if optional_size>=directory_offset+40:
            count=struct.unpack_from('<I',optional,directory_offset-4)[0]
            if count>=5:
                at,length=struct.unpack_from('<II',optional,directory_offset+32)
                certificate=bool(at and length and at+length<=size)
        return {'format':'pe32+' if magic==0x20b else 'pe32','machine':hex(machine),
            'hasEmbeddedCertificateTable':certificate,'signatureTrust':'not-verified',
            'dllCharacteristics':hex(struct.unpack_from('<H',optional,70)[0])}

def parse_kaspersky_event(path,expected_name):
    if path.is_symlink() or not path.is_file() or path.stat().st_size>1024*1024:
        raise ValueError('Expected a regular Kaspersky event XML up to 1 MiB')
    with path.open('rb') as stream:data=stream.read(1024*1024+1)
    if len(data)>1024*1024:raise ValueError('Event XML grew beyond 1 MiB during inspection')
    text=data.decode('utf-16' if data.startswith((b'\xff\xfe',b'\xfe\xff')) else 'utf-8-sig')
    if '<!DOCTYPE' in text.upper() or '<!ENTITY' in text.upper():
        raise ValueError('Event XML declarations/entities are not accepted')
    root=ET.fromstring(text);ns={'e':'http://schemas.microsoft.com/win/2004/08/events/event'}
    provider=root.find('e:System/e:Provider',ns)
    if provider is None or provider.get('Name','').casefold()!='avp':
        raise ValueError('Not a Kaspersky avp Windows event export')
    values={}
    for row in root.findall('e:EventData/e:Data',ns):
        for key,value in re.findall(r'(?m)^([A-Za-z0-9_]+): "([^"\r\n]*)"\r?$',row.text or ''):
            if key in values:raise ValueError('Ambiguous Kaspersky event field')
            values[key]=value
    digest=values.get('ProcessSha256','')
    if not re.fullmatch(r'[0-9A-Fa-f]{64}',digest):raise ValueError('Event has no exact process SHA-256')
    if PureWindowsPath(values.get('ProcessPath','')).name.casefold()!=expected_name.casefold():
        raise ValueError('Event describes a different executable; no hash substitution allowed')
    time=root.find('e:System/e:TimeCreated',ns)
    if time is None or not time.get('SystemTime'):raise ValueError('Event has no creation time')
    # Unknown vendor codes are retained only in their observed narrow formats,
    # rather than allowing a differently shaped event to carry private text.
    def code(key,pattern):
        value=values.get(key,'')
        return value if re.fullmatch(pattern,value) else ''
    # Whitelisted metadata only. Never copy command lines, machine/user names or
    # private native descriptor paths from the raw Windows export.
    return {'provider':'avp','eventId':root.findtext('e:System/e:EventID',default='',namespaces=ns),
        'recordId':root.findtext('e:System/e:EventRecordID',default='',namespaces=ns),
        'utcTime':time.get('SystemTime'),'processSha256':digest.lower(),
        'databaseVersionAsReported':code('Bases',r'\d{4}\.\d{2}\.\d{2} \d{2}:\d{2}:\d{2}:\d{3}'),
        'detectionNumber':code('Number',r'\d{1,20}'),'opaqueName':code('Name',r'[A-Fa-f0-9]{8}'),
        'confidenceCodeUninterpreted':code('Confidence',r'[A-Fa-f0-9]{8}'),'rawContentIncluded':False}

def collect(executable,detection,detected_at,*,known_sha256=None,reports=(),kaspersky_events=(),antivirus_version=None):
    if known_sha256 is not None:
        if not re.fullmatch(r'[0-9a-fA-F]{64}',known_sha256):raise ValueError('Expected an exact SHA-256 from the antivirus report')
        known_sha256=known_sha256.lower()
    events=[parse_kaspersky_event(path,executable.name) for path in kaspersky_events]
    event_hashes={event['processSha256'] for event in events}
    if len(event_hashes)>1:raise ValueError('Selected events refer to different builds; narrow the incident')
    if event_hashes:
        event_hash=next(iter(event_hashes))
        if known_sha256 and known_sha256!=event_hash:raise ValueError('Supplied hash conflicts with the original event')
        known_sha256=event_hash
    if executable.is_symlink():raise ValueError('Refuse a symbolic executable reference')
    artifact={'name':executable.name,'available':executable.is_file(),'sha256':known_sha256,'hashOrigin':'antivirus-report' if known_sha256 else 'unavailable'}
    if artifact['available']:
        before=executable.stat();actual=sha256_file(executable);pe=pe_metadata(executable);after=executable.stat()
        if (before.st_size,before.st_mtime_ns)!=(after.st_size,after.st_mtime_ns):raise ValueError('Executable changed during static inspection')
        if known_sha256 and actual!=known_sha256:raise ValueError('Available file differs from the reported detected object; do not substitute another build')
        artifact.update(sha256=actual,hashOrigin='read-existing-file',bytes=after.st_size,pe=pe)
    if events and not artifact['available']:artifact['hashOrigin']='kaspersky-event-export'
    attachments=[]
    for report in (*reports,*kaspersky_events):
        if report.is_symlink() or not report.is_file():raise ValueError('Expected an explicit regular report file')
        attachments.append({'name':report.name,'bytes':report.stat().st_size,'sha256':sha256_file(report),'contentIncluded':False})
    return {'format':'cycode-antivirus-review/1','createdAt':datetime.now(timezone.utc).isoformat(),
        'product':'CyCode Core','detection':detection,'detectedAt':detected_at,'artifact':artifact,'reports':attachments,
        'antivirusVersion':antivirus_version,'kasperskyEvents':events,
        'applicationPurpose':'Local C++ code intelligence and IDE services, native live buffers, local authenticated MCP integration and owned compiler subprocesses.',
        'incident':'Native endpoint was lost between rename Apply and Undo during real Unreal validation. Exact detected behaviour is not known.',
        'sourceFilesIncluded':False,'credentialsIncluded':False,'sampleIncluded':False,'uploaded':False,
        'falsePositiveEstablished':False,'nativeRunsSuspended':True,'supportUrl':SUPPORT,'developerProgramUrl':ALLOWLIST,
        'missing':(['Detailed antivirus event/export: component and blocked operation; Windows event codes alone do not identify the behaviour.']+
            ([] if antivirus_version else ['Antivirus product version.'])+
            ([] if artifact['sha256'] else ['Exact original sample SHA-256 from the antivirus event.'])+
            ['Vendor assessment of the original detected object; a different rebuilt executable cannot establish its safety.'])}

def write_dossier(report,output):
    output.mkdir(parents=True,exist_ok=False)
    (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    artifact=report['artifact']
    events=report.get('kasperskyEvents',[])
    event_summary='\n'.join('Windows event '+event['eventId']+', record '+event['recordId']+', UTC '+event['utcTime']+
        ', database stamp as recorded: '+event['databaseVersionAsReported'] for event in events) or 'No event export supplied'
    text=f'''Subject: Please review suspected detection of CyCode Core ({report['detection']})

Application: CyCode Core, a local development/code-intelligence service.
Detection: {report['detection']}
Reported time: {report['detectedAt']}
Antivirus product version: {report.get('antivirusVersion') or 'not supplied'}
Original Windows event evidence:
{event_summary}
Detected file name: {artifact['name']}
Exact detected SHA-256: {artifact['sha256'] or 'UNKNOWN — obtain from the original antivirus event; do not use a different build hash'}
Original executable currently available: {artifact['available']}

Observed workflow: an official SDK/MCP client requested C++ analysis and a
three-file compiler rename on an actual Unreal Engine checkout. After Apply,
the native connection was refused before Undo. Source-file hashes on disk
remained unchanged. No source save was requested. The owner supplied a matching
PDM alert. We do not know which behaviour caused the detection and are requesting
an assessment rather than claiming the sample is clean.

The service uses authenticated loopback IPC and launches compiler/search tools
with argument arrays. It uses pipes and Windows job objects to own and cancel
only children it creates. Native plugins were disabled for the recorded test.

Please identify the specific flagged behaviour and advise whether a detection
correction or an application change is appropriate.

Attach the original antivirus report only after reviewing private information.
No source files, credentials, raw logs or executable samples are in this dossier.
Nothing has been submitted or uploaded automatically.

Official procedure: {SUPPORT}
Developer allowlist program (free; eligibility conditions apply): {ALLOWLIST}
'''
    (output/'vendor-request.txt').write_text(text,encoding='utf-8')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--executable',type=Path,required=True)
    parser.add_argument('--detection',default='PDM:Trojan.Win32.Generic')
    parser.add_argument('--detected-at',required=True,help='Original report time, with timezone')
    parser.add_argument('--known-sha256',help='Exact original antivirus hash, never a substitute build')
    parser.add_argument('--report',type=Path,action='append',default=[],help='Hash an explicitly selected report; do not include its content')
    parser.add_argument('--kaspersky-event',type=Path,action='append',default=[],help='Read an explicit avp event XML; retain only whitelisted metadata')
    parser.add_argument('--antivirus-version',help='Installed product version, distinct from database stamp')
    parser.add_argument('--output',type=Path,required=True,help='New local directory; existing dossiers are preserved')
    args=parser.parse_args()
    report=collect(args.executable,args.detection,args.detected_at,known_sha256=args.known_sha256,reports=args.report,
        kaspersky_events=args.kaspersky_event,antivirus_version=args.antivirus_version)
    write_dossier(report,args.output)
    print('Local review dossier: '+str(args.output)+'; no sample executed, copied, submitted or rebuilt.')

if __name__=='__main__':main()
