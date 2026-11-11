"""Data-only validation of the actual distributed TTFs; no font runtime or GUI."""
import hashlib
import json
from pathlib import Path
import struct
import unittest

DIRECTORY=Path(__file__).resolve().parents[1]/'resources/fonts'

def mapped_glyphs(data):
    count=struct.unpack_from('>H',data,4)[0]
    tables={}
    for at in range(12,12+16*count,16):
        name,_,offset,size=struct.unpack_from('>4sIII',data,at)
        if offset+size>len(data): raise ValueError('Truncated font table')
        tables[name]=data[offset:offset+size]
    cmap=tables[b'cmap']; records=struct.unpack_from('>H',cmap,2)[0]
    for at in range(4,4+records*8,8):
        platform,encoding,offset=struct.unpack_from('>HHI',cmap,at)
        if platform!=3 or encoding!=1 or struct.unpack_from('>H',cmap,offset)[0]!=4: continue
        _,length,_,twice=struct.unpack_from('>HHHH',cmap,offset)
        table=cmap[offset:offset+length];segments=twice//2
        ends=struct.unpack_from('>'+str(segments)+'H',table,14)
        starts=struct.unpack_from('>'+str(segments)+'H',table,16+segments*2)
        deltas=struct.unpack_from('>'+str(segments)+'h',table,16+segments*4)
        offsets=struct.unpack_from('>'+str(segments)+'H',table,16+segments*6)
        result={}
        for i,(start,end,delta,extra) in enumerate(zip(starts,ends,deltas,offsets)):
            for codepoint in range(start,min(end,0xfffe)+1):
                glyph=(codepoint+delta)&0xffff if not extra else struct.unpack_from('>H',table,16+segments*6+i*2+extra+(codepoint-start)*2)[0]
                if extra and glyph: glyph=(glyph+delta)&0xffff
                if glyph: result[codepoint]=glyph
        return result
    raise ValueError('Font has no Windows Unicode BMP cmap')

class FontResourceContracts(unittest.TestCase):
    def test_distributed_inventory_matches_locked_bytes(self):
        lock=json.loads((DIRECTORY/'font-resources.json').read_text('utf-8'))
        self.assertEqual(len(lock['files']),7)
        for name,digest in lock['files'].items():
            with self.subTest(file=name):
                self.assertEqual(hashlib.sha256((DIRECTORY/name).read_bytes()).hexdigest(),digest)
                self.assertFalse((DIRECTORY/name).is_symlink())

    def test_all_public_control_glyphs_and_no_source_text_are_mapped(self):
        lock=json.loads((DIRECTORY/'font-resources.json').read_text('utf-8'))
        expected={item['codepoint'] for item in lock['icons'].values()}
        self.assertEqual(len(expected),104)
        self.assertEqual(set(mapped_glyphs((DIRECTORY/'CyCodeControls-Regular.ttf').read_bytes())),expected)
        for name in ('Roboto-Regular.ttf','Roboto-Bold.ttf','JetBrainsMono-Regular.ttf'):
            glyphs=mapped_glyphs((DIRECTORY/name).read_bytes())
            for character in 'ABCxyz012éèàçΩЖ': self.assertIn(ord(character),glyphs,(name,character))

if __name__=='__main__': unittest.main()
