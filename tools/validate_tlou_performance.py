import sys,struct
from pathlib import Path
import argparse
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'gow3_pc/scripts'),str(ROOT/'android')]
parser=argparse.ArgumentParser(description="Validate every Android performance guard and execute the resolution selector from an owned SELF")
parser.add_argument('self',type=Path)
args=parser.parse_args()
from prepare import parse_self
from tlou_patches import PERFORMANCE_RESOLUTIONS,PERFORMANCE_EFFECTS,BASE_PATCHES,BASE_SELF_SHA256
import hashlib
if hashlib.sha256(args.self.read_bytes()).hexdigest()!=BASE_SELF_SHA256:
    parser.error("SELF diferente do perfil validado")
from unicorn import Uc,UC_ARCH_X86,UC_MODE_64
from unicorn.x86_const import UC_X86_REG_RDI,UC_X86_REG_RSI,UC_X86_REG_RSP,UC_X86_REG_RAX,UC_X86_REG_XMM0,UC_X86_REG_XMM1
from unicorn.x86_const import *
elf,_,ph,*_=parse_self(args.self.read_bytes())
base=0x400000
for name,changes in {**PERFORMANCE_EFFECTS,**PERFORMANCE_RESOLUTIONS,**BASE_PATCHES}.items():
 for a,expected,_ in changes:
  p=next(p for p in ph if p['type']==1 and p['vaddr']<=a and a+len(expected)//2<=p['vaddr']+p['filesz'])
  actual=elf[p['offset']+a-p['vaddr']:p['offset']+a-p['vaddr']+len(expected)//2]
  assert actual.hex()==expected,(name,hex(a),actual.hex(),expected)
 print(name,'guards verified')
def machine(changes):
 uc=Uc(UC_ARCH_X86,UC_MODE_64)
 end=max(p['vaddr']+p['memsz'] for p in ph if p['type']==1)
 uc.mem_map(base,(end+0xfff)&~0xfff)
 for p in ph:
  if p['type']==1:uc.mem_write(base+p['vaddr'],bytes(elf[p['offset']:p['offset']+p['filesz']]))
 for at,_,replacement in changes:
  if replacement:uc.mem_write(base+at,bytes.fromhex(replacement))
 uc.mem_map(0x8000000,0x4000)
 uc.reg_write(UC_X86_REG_RSP,0x8003000)
 uc.reg_write(UC_X86_REG_RBP,0x8002800)
 uc.reg_write(UC_X86_REG_RDI,0x8000000)
 return uc

def dimensions(uc,w,h,address=0x8000000,aspect=16/9):
 data=uc.mem_read(address,0x100)
 for offset in (8,0x18,0x24,0x38):
  assert struct.unpack_from('<II',data,offset)==(w,h),(hex(offset),data.hex())
 assert struct.unpack_from('<II',data,0x40)==(w//8,h//4)
 # Unicorn 2.1.4 does not execute VMULSS/VDIVSS reliably; the native
 # x86-64 validation below checks this arithmetic on real instructions.
 assert abs(struct.unpack_from('<f',data,0x20)[0]-aspect)<2e-7
 return data

def native_setters(changes,w,h):
 import ctypes,mmap,platform
 if platform.machine().lower() not in ('x86_64','amd64'):
  raise RuntimeError('Full arithmetic validation requires an x86-64 AVX host')
 end=max(p['vaddr']+p['memsz'] for p in ph if p['type']==1)
 memory=mmap.mmap(-1,(end+0xfff)&~0xfff,prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC)
 start=ctypes.addressof(ctypes.c_char.from_buffer(memory))
 for p in ph:
  if p['type']==1:memory[p['vaddr']:p['vaddr']+p['filesz']]=elf[p['offset']:p['offset']+p['filesz']]
 for at,_,replacement in changes:
  if replacement:memory[at:at+len(bytes.fromhex(replacement))]=bytes.fromhex(replacement)
 # Isolate setters: do not run the external notification routine or any game loop.
 memory[0x9f5b30]=0xc3
 ctor=ctypes.CFUNCTYPE(None,ctypes.c_void_p)(start+0x9103a0)
 enum=ctypes.CFUNCTYPE(None,ctypes.c_void_p,ctypes.c_int,ctypes.c_int)(start+0x9104c0)
 custom=ctypes.CFUNCTYPE(None,ctypes.c_void_p,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_float)(start+0x910740)
 obj=ctypes.create_string_buffer(256)
 def check(aspect):
  data=obj.raw
  for offset in (8,0x18,0x24,0x38):assert struct.unpack_from('<II',data,offset)==(w,h)
  assert struct.unpack_from('<II',data,0x40)==(w//8,h//4)
  assert abs(struct.unpack_from('<f',data,0x10)[0]-aspect*h/w)<2e-7
  assert abs(struct.unpack_from('<f',data,0x20)[0]-aspect)<2e-7
 ctor(obj);check(16/9)
 for mode in range(5):
  for rate in (0,1,2,4):enum(obj,mode,rate);check(1.0)
 for width,height in ((1920,1080),(1168,657),(1024,600)):
  custom(obj,width,height,4,16/9);check(16/9)
 del ctor,enum,custom
 memory.close()

for res,changes in PERFORMANCE_RESOLUTIONS.items():
 w,h={'720':(1280,720),'540':(960,540),'360':(640,360)}[res]
 native_setters(changes,w,h)
 uc=machine(changes)
 uc.emu_start(base+0x9103a0,base+0x91046c,count=200)
 data=dimensions(uc,w,h)
 # Preserve constructor defaults unrelated to scene dimensions.
 assert struct.unpack_from('<II',data,0x30)==(2560,1216)
 assert struct.unpack_from('<I',data,0x74)[0]==1
 for mode in (0,1,2,3,4):
  for rate in (0,1,2,4):
   uc=machine(changes)
   uc.mem_write(0x8000000+0x74,struct.pack('<I',1))
   uc.reg_write(UC_X86_REG_RSI,mode)
   from unicorn.x86_const import UC_X86_REG_RDX
   uc.reg_write(UC_X86_REG_RDX,rate)
   uc.reg_write(UC_X86_REG_XMM0,(1<<128)-1);uc.reg_write(UC_X86_REG_XMM1,(1<<128)-1)
   # Execute entry, mode dispatch, stores and shared tail, stopping only at
   # the external notification routine. Old tests skipped entry and dispatch.
   uc.emu_start(base+0x9104c0,base+0x9f5b30,count=200)
   data=dimensions(uc,w,h,aspect=1.0)
   assert struct.unpack_from('<I',data,0x84)[0]==mode
   assert struct.unpack_from('<I',data,0x74)[0]==(rate or 1)
   floats=[struct.unpack('<f',struct.pack('<I',uc.reg_read(r)&0xffffffff))[0] for r in [UC_X86_REG_XMM0,UC_X86_REG_XMM1]]
   assert floats==[float(w),float(h)],floats

 # Custom-size setter (may be reached through indirect calls), preserving the
 # caller's aspect and update rate, but forcing requested dimensions.
 for input_w,input_h in ((1920,1080),(1168,657),(1024,600)):
  uc=machine(changes)
  uc.reg_write(UC_X86_REG_RSI,input_w);uc.reg_write(UC_X86_REG_RDX,input_h)
  uc.reg_write(UC_X86_REG_RCX,4)
  uc.reg_write(UC_X86_REG_XMM0,struct.unpack('<I',struct.pack('<f',16/9))[0])
  uc.emu_start(base+0x910740,base+0x9f5b30,count=200)
  dimensions(uc,w,h)
  assert struct.unpack('<I',uc.mem_read(0x8000000+0x74,4))[0]==4
 # Saved-settings/global fallback. This was completely absent from v0.9.
 uc=machine(changes)
 uc.emu_start(base+0x910b00,base+0x910bda,count=200)
 dimensions(uc,w,h,base+0x181aa10)
 # Scene allocation consumer: reads are replaced with selected scene sizes.
 uc=machine(changes)
 uc.emu_start(base+0x91adbc,base+0x91adbc+len(bytes.fromhex(next(c[1] for c in changes if c[0]==0x91adbc))),count=50)
 assert uc.reg_read(UC_X86_REG_RBX)==w and uc.reg_read(UC_X86_REG_R12)==h
 assert struct.unpack('<I',uc.mem_read(0x8002800-0x74,4))[0]==w
 assert struct.unpack('<I',uc.mem_read(0x8002800-0x78,4))[0]==h
 # Four native output/AA allocations: never shrink them to the scene size.
 for at,expected in ((0x92ef1a,(1920,1080,0x3d)),(0x92ef7d,(960,540,0x31)),
                     (0x92efc0,(1920,1080,0x3d)),(0x92f00d,(960,540,0x31))):
  uc=machine(changes)
  uc.reg_write(UC_X86_REG_R12,333);uc.reg_write(UC_X86_REG_R14,444)
  uc.reg_write(UC_X86_REG_R13,555);uc.reg_write(UC_X86_REG_RBX,666)
  uc.emu_start(base+at,base+0x9cc310,count=100)
  assert tuple(uc.reg_read(r) for r in (UC_X86_REG_RCX,UC_X86_REG_R8,UC_X86_REG_R9))==expected
  sp=uc.reg_read(UC_X86_REG_RSP)
  assert struct.unpack('<QQQ',uc.mem_read(sp+8,24))==(7,0,1)
 # Output dimensions and movie viewport stay native, independently of 3D scene.
 uc=machine(changes)
 uc.emu_start(base+0x91f6e8,base+0x91f6f7,count=50)
 assert uc.reg_read(UC_X86_REG_R15)==1920 and uc.reg_read(UC_X86_REG_R12)==1080
 uc=machine(changes)
 uc.emu_start(base+0x8e441c,base+0x9913f0,count=50)
 assert uc.reg_read(UC_X86_REG_RSI)==1920 and uc.reg_read(UC_X86_REG_RDX)==1080
 uc=machine(changes)
 uc.emu_start(base+0x91a37a,base+0x91a391,count=50)
 for reg,value in ((UC_X86_REG_XMM2,1920.0),(UC_X86_REG_XMM3,1080.0)):
  assert struct.unpack('<f',struct.pack('<I',uc.reg_read(reg)&0xffffffff))[0]==value
 uc=machine(changes)
 uc.emu_start(base+0x9d4023,base+0x9d4047,count=50)
 for reg,value in ((UC_X86_REG_XMM0,1/1080),(UC_X86_REG_XMM1,1/1920)):
  actual=struct.unpack('<f',struct.pack('<I',uc.reg_read(reg)&0xffffffff))[0]
  assert abs(actual-value)<1e-9,(reg,actual,value)
 print(res,'Bachata full profile: constructor, 20 enum cases, custom setter, saved settings, scene allocation, output/AA and movie paths passed:',w,h)
