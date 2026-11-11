"""Derive the AI method catalogue from the executable operation contracts."""
import argparse
import hashlib
import json
from pathlib import Path
import sys

APP=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(APP))
from contracts import METHODS, OPERATIONS

def catalogue():
    manifest=json.loads((APP/'CyTool.json').read_text('utf-8'))
    operations=manifest['operations']
    assert len(METHODS)==len(OPERATIONS)==len(operations)
    assert {op['id'] for op in operations}==set(METHODS)
    # Validate complete schemas, permissions and capabilities against the same
    # source used by the official SDK. Never invent a parallel MCP transport.
    for identity,method,description,inputs,outputs,permissions in OPERATIONS:
        op=next(op for op in operations if op['id']==identity)
        assert op['inputSchema']==inputs and op['outputSchema']==outputs and op['permissions']==permissions
    return {'format':'cycode-agent-methods/1','toolId':manifest['id'],
        'source':'app/CyTool.json + app/contracts.py',
        'contractSha256':hashlib.sha256(json.dumps(operations,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
        'transport':'Official CyToolsCore MCP adapter; op_<id> submits a job, not a synchronous domain result.',
        'guide':'Docs/Agents.md','persistentSources':'Workspace paths are persistent native document references, never SDK temporary input files.',
        'discovery':['cy_describe','cy_list_operations','cy_describe_operation','cy_can_run'],
        'jobs':['cy_get_job','cy_cancel_job','cy_events'],
        'operationCount':len(operations),
        'operations':[{'mcpTool':'op_'+op['id'],'nativeMethod':METHODS[op['id']],
            'documentation':'operation-'+op['id'].replace('_','-')+'-'+op['id'].replace('_','-'),**op}
            for op in operations]}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check',action='store_true',help='Verify the checked-in catalogue without changing files')
    args=parser.parse_args()
    path=APP/'MCPMethods.json';expected=json.dumps(catalogue(),ensure_ascii=False,indent=2)+'\n'
    if args.check:
        if not path.is_file() or path.read_text('utf-8')!=expected:raise SystemExit('Regenerate app/MCPMethods.json with this script')
    else:path.write_text(expected,encoding='utf-8',newline='\n')
    print(str(len(OPERATIONS))+' native operations: '+('catalogue verified' if args.check else 'catalogue generated'))

if __name__=='__main__':main()
