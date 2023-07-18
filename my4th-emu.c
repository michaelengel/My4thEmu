#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

uint8_t mem[65536];

uint8_t  opcode;
uint8_t  param1;
uint8_t  param2;
uint8_t  regs[256];
uint8_t  f, r; // for temporary flag calculations
uint16_t r16;

// Register name aliases
enum { TEMP1 = 0, TEMP2, TEMP3, TEMP4, TEMP5, PAR1, PAR2, SP,
       PC_L, PC_H, LR_L, LR_H, PTR_L, PTR_H, ACCU, FLAG,
       R0, R1, R2, R3, R4_L, R4_H, R5_L, R5_H,
       R6_L, R6_H, R7_L, R7_H, OUTP }; 

uint8_t r8(uint16_t addr) { 
  return mem[addr];
}

void w8(uint16_t addr, uint8_t data) { 
  if (addr >= 0x8000) 
    mem[addr] = data;
  else
    fprintf(stderr, "Write attempt to ROM at 0x%04x\n", addr);
}

void disasm(uint16_t pc, uint8_t opcode, uint8_t p1, uint8_t p2) {
  char s[80];
  printf("%04x: %02x %02x %02x  ---  ", pc, opcode, p1, p2);
  fflush(stdout);
  sprintf(s, "grep \"^    %04X\" rom.lst", pc);
  system(s);
  fflush(stdout);
}

uint16_t r16stack(void) {
  // TODO: SP should wrap around at 0x81FF!
  uint16_t val;
  regs[SP] -= 2;
  printf("r16stack sp = %02x, (sp) = %04x\n", regs[SP], r8(0x8100 + regs[SP]) + (r8(0x8100 + regs[SP]+1) * 256));
  val = r8(0x8100 + regs[SP]) + (r8(0x8100 + regs[SP]+1) * 256);
  return val;
}

void push(uint8_t val) {
  w8(0x8100 + regs[SP], val);
  printf("      PUSH sp = %02x, (sp) = %02x\n", regs[SP], r8(0x8100 + regs[SP]));
  regs[SP]++;
}

uint16_t pop(void) {
  uint8_t val;
  val = r8(0x8100 + regs[SP]);
  printf("      POP sp = %02x, (sp) = %02x\n", regs[SP], r8(0x8100 + regs[SP]));
  regs[SP]--;
  return val;
}

uint16_t rpc(void) {
  return regs[PC_L] + (regs[PC_H] << 8);
}

void wpc(uint16_t pc) {
  regs[PC_L] = pc & 0xff;
  regs[PC_H] = pc >> 8;
}

void load(void) {
  int f = open("rom.bin", O_RDONLY);
  int n = read(f, mem, 32768); 
  if (n < 0) { perror("open"); }
  if (n < 32768) { printf("short read: %d\n", n); }
}

int main(int argc, char **argv) {
  regs[PC_L] = 0x00;
  regs[PC_H] = 0x2D;
  regs[SP]   = 0x00;

  load();

  while(1) {
  opcode = r8(rpc());
  param1 = r8(rpc()+1);
  param2 = r8(rpc()+2);

  if (1) disasm(rpc(), opcode, param1, param2);

  switch(opcode) {
    case 0x00: // RST (reset) - 1 byte
      regs[SP] = 0;
      wpc(0x2d00);
      break;
    case 0x01: // LD r,# (load register) - 3 bytes
      regs[param1] = param2;
      wpc(rpc()+3);
      break;
    case 0x02: // LD r,r (load register)
      regs[param1] = regs[param2];
      wpc(rpc()+3);
      break;
    case 0x03: // LDA # (load accumulator) - 2 bytes
      regs[ACCU] = param1;
      wpc(rpc()+2);
      break;
    case 0x04: // LDA r (load accumulator) - 2 bytes
      regs[ACCU] = regs[param1];
      wpc(rpc()+2);
      break;
    case 0x05: // LAP (load accumulator through pointer) - 1 byte
      regs[ACCU] = r8(regs[PTR_L]+(regs[PTR_H]<<8));
      wpc(rpc()+1);
      break;
    case 0x06: // STA r (store accumulator) - 2 bytes
      regs[param1] = regs[ACCU];
      wpc(rpc()+2);
      break;
    case 0x07: // SAP (store accumulator through pointer) - 1 byte
      w8(regs[PTR_L]+(regs[PTR_H]<<8), regs[ACCU]);
      wpc(rpc()+1);
      break;
    case 0x24: // AD r (add without carry) - 2 bytes
      regs[ACCU] += regs[param1];
      r16 = (uint16_t)regs[ACCU]+(uint16_t)regs[param1];
      if (r16 > 255) regs[FLAG] = 1; else regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x25: // SU r (subtract without carry) - 2 bytes
      regs[ACCU] -= regs[param1];
      r16 = (uint16_t)regs[ACCU]-(uint16_t)regs[param1];
      if (r16 > 255) regs[FLAG] = 1; else regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x10: // ADD r (add with carry) - 2 bytes
      regs[ACCU] += regs[param1];
      if (regs[FLAG]) regs[ACCU]++;
      r16 = (uint16_t)regs[ACCU]+(uint16_t)regs[param1];
      if (regs[FLAG]) r++;
      if (r16 > 255) regs[FLAG] = 1; else regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x11: // SUB r (sub with borrow) - 2 bytes
      regs[ACCU] -= regs[param1];
      if (regs[FLAG]) regs[ACCU]--;
      r16 = (uint16_t)regs[ACCU]-(uint16_t)regs[param1];
      if (regs[FLAG]) r--;
      if (r16 > 255) regs[FLAG] = 0; else regs[FLAG] = 1;
      wpc(rpc()+2);
      break;
    case 0x14: // CMP r (compare) - 2 bytes
      if (regs[ACCU] == regs[param1])
        regs[FLAG] = 1;
      else
        regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x13: // CMP # (compare immediate) - 2 bytes
      if (regs[ACCU] == param1)
        regs[FLAG] = 1;
      else
        regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x15: // TST r (test) - 2 bytes
      if (regs[param1] == 0)
        regs[FLAG] = 1;
      else
        regs[FLAG] = 0;
      wpc(rpc()+2);
      break;
    case 0x0d: // AND r (log. and) - 2 bytes
      regs[ACCU] &= regs[param1];
      wpc(rpc()+2);
      break;
    case 0x0e: // OR r (log. or) - 2 bytes
      regs[ACCU] |= regs[param1];
      wpc(rpc()+2);
      break;
    case 0x0f: // XOR r (log. xor) - 2 bytes
      regs[ACCU] ^= regs[param1];
      wpc(rpc()+2);
      break;
    case 0x0b: // ROL r (rotate left through FLAG) - 2 bytes
      f = regs[param1] >> 7;
      regs[param1] = ((regs[FLAG]&1)) | (regs[param1] << 1);
      regs[FLAG] = f & 1;
      wpc(rpc()+2);
      break;
    case 0x0c: // ROR r (rotate right through FLAG) - 2 bytes
      f = regs[param1] & 1;
      regs[param1] = ((regs[FLAG]&1)<<8) | (regs[param1] >> 1);
      regs[FLAG] = f & 1;
      wpc(rpc()+2);
      break;
    case 0x21: // RWL r (rotate word left through FLAG) - 2 bytes
      f = regs[param2] >> 7;
      r = regs[param1];
      regs[param1]   = (regs[param1] << 1) | (f & 0x01);
      regs[param1+1] = (regs[param1+1] << 1) | ((regs[param1]&0x80)>>7);
      regs[FLAG] = f & 1;
      wpc(rpc()+2);
      break;
    case 0x0a: // DEC r (decrement) - 2 bytes
      regs[param1]--;
      wpc(rpc()+2);
      break;
    case 0x09: // INC r (increment) - 2 bytes
      regs[param1]++;
      wpc(rpc()+2);
      break;
    case 0x16: // JPF abs (conditional jump) - 3 bytes
      if (regs[FLAG] != 0) 
        wpc(param1 + (param2 << 8)); 
      else
        wpc(rpc()+3);
      break;
    case 0x17: // JNF abs (conditional jump) - 3 bytes
      if (regs[FLAG] == 0) 
        wpc(param1 + (param2 << 8)); 
      else
        wpc(rpc()+3);
      break;
    case 0x18: // JMP abs (unconditional jump) - 3 bytes
      wpc(param1 + (param2 << 8)); 
      break;
    case 0x19: // JSR abs (call subroutine) - 3 bytes
      regs[LR_L] = (rpc() + 3) & 0xff;
      regs[LR_H] = (rpc() + 3) >> 8;
      wpc(param1 + (param2 << 8)); 
      break;
    case 0x1a: // RET (return from subroutine) - 1 byte
      wpc(regs[LR_L]+(regs[LR_H]*256));
      break;
    case 0x1f: // RTS (return from subroutine w/stack) - 1 byte
      r16 = r16stack();
      regs[LR_L] = r16 & 0xff;
      regs[LR_H] = r16 >> 8;
      wpc(regs[LR_L]+(regs[LR_H]*256));
      break;
    case 0x20: // JLP abs (jump in a loop) - 3 bytes
      regs[R0]--;
      if (regs[R0] != 0) 
        wpc(param1 + (param2 << 8)); 
      else
        wpc(rpc()+3);
      break;
    case 0x1d: // IN port (input) - 1 byte
      regs[ACCU] = 0x42; // TODO: simulate I/O
      wpc(rpc()+1);
      break;
    case 0x1e: // PHL (push link register to stack) - 1 byte
      push(regs[LR_L]);
      push(regs[LR_H]);
      wpc(rpc()+1);
      break;
    case 0x1b: // PSH reg (push register to stack) - 2 bytes
      push(regs[param1]);
      wpc(rpc()+2);
      break;
    case 0x22: // SEC (set flag register) - 1 byte
      regs[FLAG] = 1;
      wpc(rpc()+1);
      break;
    case 0x23: // CLC (clear flag register) - 1 byte
      regs[FLAG] = 0;
      wpc(rpc()+1);
      break;
    case 0x1c: // POP reg (push register to stack) - 2 bytes
      regs[param1] = pop();
      wpc(rpc()+2);
      break;
    case 0x26: // OUT port (output) - 2 bytes
      printf("      OUT: 0x%02x\n", regs[param1]);
      wpc(rpc()+2);
      break;
    default: 
      printf("Unknown opcode 0x%02x at 0x%04x\n", opcode, rpc());
      break;
  }
  }
}
