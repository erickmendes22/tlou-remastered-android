"""Patch selection, atomic failure and restore behavior without a game dump."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch, Mock

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import tlou_patches as profiles
ROOT=Path(__file__).resolve().parents[2]

def fixture(root):
    (root/'tlou-view').mkdir()
    (root/'tlou-view/eboot.bin').write_bytes(b'owned synthetic SELF fixture')
    changes=[line for group in [*profiles.BASE_PATCHES.values(),*profiles.PERFORMANCE_EFFECTS.values(),*profiles.PERFORMANCE_RESOLUTIONS.values()] for line in group]
    size=max(at+len(expected)//2 for at,expected,_ in changes)+4096
    elf=bytearray(size+4096)
    struct.pack_into('<Q',elf,0x20,0x40)
    struct.pack_into('<HH',elf,0x36,56,1)
    struct.pack_into('<IIQQQQQQ',elf,0x40,1,5,4096,0,0,size,size,4096)
    written={}
    for at,expected,_ in changes:
        for i,b in enumerate(bytes.fromhex(expected)):
            if at+i in written: assert written[at+i]==b
            written[at+i]=b
        elf[4096+at:4096+at+len(expected)//2]=bytes.fromhex(expected)
    (root/'eboot.elf').write_bytes(elf)

def entries(path):
    data=path.read_bytes();assert data[:8]==b'G3PATCH2'
    count=struct.unpack_from('<Q',data,16)[0];out={};off=24
    for _ in range(count):
        at,size=struct.unpack_from('<QQ',data,off);off+=16
        out[at]=data[off:off+size];off+=size
    assert off==len(data)
    return out

class PerformancePatches(unittest.TestCase):
    def trusted(self): return patch.object(profiles.hashlib,'sha256',return_value=Mock(hexdigest=lambda:profiles.BASE_SELF_SHA256))

    def test_mutually_exclusive_resolution_and_strict_options(self):
        for value in ['720+540',720,['720','540']]:
            with self.assertRaises(ValueError): profiles.selected_options({'resolution':value})
        with self.assertRaises(ValueError): profiles.selected_options({'ssao':1})
        self.assertEqual(list(profiles.selected_options({'resolution':'540'})),['resolution-540'])

    def test_select_and_restore_without_touching_image(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);fixture(root)
            before=(root/'eboot.elf').read_bytes()
            options={key:True for key in profiles.PERFORMANCE_EFFECTS};options['resolution']='360'
            with self.trusted(): profiles.write_selected(ROOT,root,options)
            selected=entries(root/'patches.bin')
            self.assertTrue(selected[0x910740].startswith(bytes.fromhex('be80020000ba68010000')))
            self.assertEqual(selected[0x910b22],struct.pack('<I',640))
            self.assertEqual(selected[0x910b29],struct.pack('<I',360))
            self.assertIn(0x91adbc,selected)
            self.assertIn(0x92ef1a,selected)
            self.assertEqual(selected[0x946363],b'\0')
            self.assertEqual((root/'eboot.elf').read_bytes(),before)
            with self.trusted(): profiles.write_selected(ROOT,root,{})
            restored=entries(root/'patches.bin')
            self.assertNotIn(0x910740,restored);self.assertNotIn(0x910b22,restored);self.assertNotIn(0x946363,restored)
            self.assertEqual(json.loads((root/'patch-validation.json').read_text())['selected'],[])

    def test_complete_bachata_027_write_parity(self):
        # Hash of ordered VA/length/bytes extracted from the actual 0.2.7 APK,
        # TheLastOfUsRemastered.xml. Protects all 32 writes, including native
        # movie/output separation, rather than just the preset immediates.
        golden={'720':'7ececeace75c9ce869b9635d137ba0f3decee1fc45daea545149d2476c2c1283',
                '540':'abaeb0dd67969b7c43293962e14a0d60fbcd3a9093b8df4a4ad54cfeb03e307b',
                '360':'a2a8ecfdd42436ed9fb200796a335d8a254bb365dbd3ca10ed7ea82826e3a522'}
        for name,changes in profiles.PERFORMANCE_RESOLUTIONS.items():
            writes=[(at,bytes.fromhex(new)) for at,old,new in changes if new]
            self.assertEqual(len(writes),32)
            blob=b''.join(struct.pack('<QQ',at,len(data))+data for at,data in writes)
            self.assertEqual(hashlib.sha256(blob).hexdigest(),golden[name])

    def test_bad_instruction_preserves_previous_patch_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);fixture(root)
            with self.trusted(): profiles.write_selected(ROOT,root,{})
            previous=(root/'patches.bin').read_bytes()
            with (root/'eboot.elf').open('r+b') as stream:
                stream.seek(4096+0x946363);stream.write(b'\x02')
            with self.trusted(),self.assertRaisesRegex(ValueError,'Assinatura divergente'):
                profiles.write_selected(ROOT,root,{'ssao':True})
            self.assertEqual((root/'patches.bin').read_bytes(),previous)

    def test_unknown_self_cannot_enable_known_patches(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);fixture(root)
            with self.assertRaisesRegex(ValueError,'validado'):
                profiles.write_selected(ROOT,root,{'resolution':'720'})
            self.assertFalse((root/'patches.bin').exists())

if __name__=='__main__': unittest.main()
