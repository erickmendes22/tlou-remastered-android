"""Reproduce the supplied NULL+0x20 fault and check valid-input equivalence.
Requires the owned SELF, Unicorn and an x86-64 AVX host; no game files shipped.
"""
import argparse,ctypes,hashlib,mmap,platform,random,struct,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'gow3_pc/scripts'),str(ROOT/'android')]
from prepare import parse_self
from tlou_patches import BASE_SELF_SHA256,BASE_PATCHES
from unicorn import Uc, UcError, UC_ARCH_X86, UC_MODE_64, UC_HOOK_MEM_INVALID
from unicorn.x86_const import *
p=argparse.ArgumentParser();p.add_argument('self',type=Path);args=p.parse_args()
raw=args.self.read_bytes();assert hashlib.sha256(raw).hexdigest()==BASE_SELF_SHA256
elf,_,segments,*_=parse_self(raw)
end=max(s['vaddr']+s['memsz'] for s in segments if s['type']==1)
size=(end+4095)&~4095
patch=BASE_PATCHES['null-component-transforms']
for at,old,new in patch:
 s=next(s for s in segments if s['type']==1 and s['vaddr']<=at<at+len(old)//2<=s['vaddr']+s['filesz'])
 off=s['offset']+at-s['vaddr'];assert elf[off:off+len(old)//2].hex()==old
for patched in (False,True):
 u=Uc(UC_ARCH_X86,UC_MODE_64);base=0x400000;u.mem_map(base,size)
 for s in segments:
  if s['type']==1:u.mem_write(base+s['vaddr'],bytes(elf[s['offset']:s['offset']+s['filesz']]))
 if patched:
  for at,_,new in patch:
   if new:u.mem_write(base+at,bytes.fromhex(new))
 u.mem_map(0x8000000,0x10000);u.mem_write(0x8007000,struct.pack('<Q',0x800f000))
 u.reg_write(UC_X86_REG_RSP,0x8007000);u.reg_write(UC_X86_REG_RBP,0x8007100)
 u.reg_write(UC_X86_REG_RDI,0x8000000);u.reg_write(UC_X86_REG_RSI,0x8003000)
 u.reg_write(UC_X86_REG_RDX,0x8002000);u.reg_write(UC_X86_REG_RCX,0)
 u.mem_write(0x8002000,struct.pack('<QI',0x8001000,1))
 before=bytes(u.mem_read(0x8000000,0x3000));invalid=[]
 def fault(uc,access,address,length,value,data):
  invalid.append(address);return False
 u.hook_add(UC_HOOK_MEM_INVALID,fault)
 try:u.emu_start(base+0xf6be70,0x800f000,count=200)
 except UcError:
  assert not patched and invalid==[0x20]
  print('Original: reproduced fault at NULL+0x20')
 else:
  assert patched and not invalid
  assert before==bytes(u.mem_read(0x8000000,0x3000))
  assert u.reg_read(UC_X86_REG_RBP)==0x8007100
  print('Patched: absent source returns safely; destination and callee state unchanged')
assert platform.machine().lower() in ('x86_64','amd64')
def native(patched):
 memory=mmap.mmap(-1,size,prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC)
 start=ctypes.addressof(ctypes.c_char.from_buffer(memory))
 for s in segments:
  if s['type']==1:memory[s['vaddr']:s['vaddr']+s['filesz']]=elf[s['offset']:s['offset']+s['filesz']]
 if patched:
  for at,_,new in patch:
   if new:memory[at:at+len(bytes.fromhex(new))]=bytes.fromhex(new)
 fn=ctypes.CFUNCTYPE(None,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p)(start+0xf6be70)
 return memory,fn
original,old=native(False);changed,new=native(True)
rng=random.Random(17)
for case in range(48):
 count=case%4
 base=ctypes.create_string_buffer(struct.pack('<8f',rng.random(),rng.random(),rng.random(),1,0,0,0,1))
 source=ctypes.create_string_buffer(max(16,count*48))
 for i in range(count):struct.pack_into('<8fi',source,i*48,rng.random(),rng.random(),rng.random(),1,0,0,0,1,-1)
 first=ctypes.create_string_buffer(bytes([0x5a])*1024,1024);second=ctypes.create_string_buffer(first.raw,1024)
 d1=ctypes.create_string_buffer(struct.pack('<QI',ctypes.addressof(first),count))
 d2=ctypes.create_string_buffer(struct.pack('<QI',ctypes.addressof(second),count))
 old(base,None,d1,source);new(base,None,d2,source)
 assert first.raw==second.raw,case
print('Native: 48 valid-input cases match original output byte for byte')
del old,new;original.close();changed.close()
