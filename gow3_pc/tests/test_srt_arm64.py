"""Run emitted ARM64 instructions and compare with the production interpreter."""
from pathlib import Path
import json
import random
import struct
import subprocess
import tempfile
import unittest
from unicorn import Uc, UC_ARCH_ARM64, UC_MODE_ARM
from unicorn.arm64_const import UC_ARM64_REG_X0, UC_ARM64_REG_X1, UC_ARM64_REG_SP, UC_ARM64_REG_LR

# Stable persisted operation numbers from srt_arm64.h.
END,PUSH,DYN_PUSH,POP,COPY,DYN_COPY,USER,FLAT,IMM,ADD,SUB,MUL,SHL,SHR,AND,OR,XOR,NOT,UMIN,UMAX,BFE=range(21)
MAGIC=0x42545253

def packed(words): return struct.pack('<'+'I'*len(words),*words)

class Arm64Srt(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory()
        cls.runner=Path(cls.temp.name)/'srt'
        subprocess.run(['g++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(Path(__file__).with_name('srt_arm64_runner.cpp')),'-o',str(cls.runner)],check=True)

    @classmethod
    def tearDownClass(cls): cls.temp.cleanup()

    def result(self, program):
        return json.loads(subprocess.check_output([str(self.runner)],input=' '.join(map(str,program)).encode()))

    def execute(self, program):
        data=self.result(program)
        self.assertIn('code',data)
        uc=Uc(UC_ARCH_ARM64,UC_MODE_ARM)
        uc.mem_map(0x10000,0x60000)
        uc.mem_write(0x10000,packed(data['code']))
        uc.mem_write(0x20000,packed(data['user']))
        uc.mem_write(0x21000,packed([0xdeadbeef]*256))
        uc.mem_write(0x30000,packed(data['table']))
        uc.mem_write(0x40000,packed(data['child']))
        uc.reg_write(UC_ARM64_REG_X0,0x20000)
        uc.reg_write(UC_ARM64_REG_X1,0x21000)
        uc.reg_write(UC_ARM64_REG_SP,0x68000)
        uc.reg_write(UC_ARM64_REG_LR,0x18000)
        uc.emu_start(0x10000,0x18000,count=100000)
        actual=list(struct.unpack('<256I',uc.mem_read(0x21000,1024)))
        self.assertEqual(actual,data['expected'])
        self.assertEqual(uc.reg_read(UC_ARM64_REG_SP),0x68000)
        return actual

    def test_nested_and_high_pointer_bits(self):
        result=self.execute([MAGIC,PUSH,0,COPY,4,16,PUSH,8,COPY,5,17,POP,COPY,6,18,POP,END])
        self.assertEqual(result[16:19],[159,374,233])

    def test_userdata_is_independent_of_flat(self):
        result=self.execute([MAGIC,PUSH,0,USER,2,DYN_COPY,16,IMM,4,DYN_COPY,17,FLAT,16,IMM,122,SUB,DYN_COPY,18,POP,END])
        self.assertEqual(result[16:19],[122,159,11])

    def test_arithmetic_and_shift_wrapping(self):
        rng=random.Random(552)
        for op in [ADD,SUB,MUL,SHL,SHR,AND,OR,XOR,UMIN,UMAX]:
            for _ in range(8):
                a,b=rng.getrandbits(32),rng.getrandbits(32)
                with self.subTest(op=op,a=a,b=b):
                    self.execute([MAGIC,PUSH,0,IMM,a,IMM,b,op,IMM,63,AND,DYN_COPY,16,POP,END])
        self.execute([MAGIC,PUSH,0,USER,2,NOT,IMM,63,AND,DYN_COPY,16,POP,END])
        self.execute([MAGIC,PUSH,0,IMM,0xabcdef12,IMM,4,IMM,5,BFE,DYN_COPY,16,POP,END])

    def test_dynamic_pointer_and_multiple_roots(self):
        self.execute([MAGIC,USER,2,IMM,3,SUB,DYN_PUSH,COPY,2,16,IMM,8,DYN_PUSH,COPY,3,17,POP,POP,PUSH,0,COPY,1,18,POP,END])

    def test_malformed_cache_programs(self):
        for program in [[],[MAGIC],[MAGIC,POP,END],[MAGIC,IMM,1,END],[MAGIC,PUSH,0,END],[MAGIC,USER,16,NOT,DYN_COPY,16,END],[MAGIC,999,END],[MAGIC,PUSH,0,COPY],[MAGIC,END,END]]:
            with self.subTest(program=program): self.assertEqual(self.result(program),{})

if __name__=='__main__': unittest.main()
