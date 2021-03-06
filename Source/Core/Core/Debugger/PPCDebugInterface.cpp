// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/Debugger/PPCDebugInterface.h"

#include <cstddef>
#include <string>

#include "Common/Align.h"
#include "Common/GekkoDisassembler.h"
#include "Common/StringUtil.h"

#include "Core/Core.h"
#include "Core/HW/DSP.h"
#include "Core/PowerPC/MMU.h"
#include "Core/PowerPC/PPCSymbolDB.h"
#include "Core/PowerPC/PowerPC.h"

void PPCPatches::Patch(std::size_t index)
{
  auto& patch = m_patches[index];
  if (patch.value.empty())
    return;

  const u32 address = patch.address;
  const std::size_t size = patch.value.size();
  if (!PowerPC::HostIsRAMAddress(address))
    return;

  for (u32 offset = 0; offset < size; ++offset)
  {
    const u8 value = PowerPC::HostRead_U8(address + offset);
    PowerPC::HostWrite_U8(patch.value[offset], address + offset);
    patch.value[offset] = value;

    if (((address + offset) % 4) == 3)
      PowerPC::ScheduleInvalidateCacheThreadSafe(Common::AlignDown(address + offset, 4));
  }
  if (((address + size) % 4) != 0)
  {
    PowerPC::ScheduleInvalidateCacheThreadSafe(
        Common::AlignDown(address + static_cast<u32>(size), 4));
  }
}

std::size_t PPCDebugInterface::SetWatch(u32 address, const std::string& name)
{
  return m_watches.SetWatch(address, name);
}

const Common::Debug::Watch& PPCDebugInterface::GetWatch(std::size_t index) const
{
  return m_watches.GetWatch(index);
}

const std::vector<Common::Debug::Watch>& PPCDebugInterface::GetWatches() const
{
  return m_watches.GetWatches();
}

void PPCDebugInterface::UnsetWatch(u32 address)
{
  m_watches.UnsetWatch(address);
}

void PPCDebugInterface::UpdateWatch(std::size_t index, u32 address, const std::string& name)
{
  return m_watches.UpdateWatch(index, address, name);
}

void PPCDebugInterface::UpdateWatchAddress(std::size_t index, u32 address)
{
  return m_watches.UpdateWatchAddress(index, address);
}

void PPCDebugInterface::UpdateWatchName(std::size_t index, const std::string& name)
{
  return m_watches.UpdateWatchName(index, name);
}

void PPCDebugInterface::EnableWatch(std::size_t index)
{
  m_watches.EnableWatch(index);
}

void PPCDebugInterface::DisableWatch(std::size_t index)
{
  m_watches.DisableWatch(index);
}

bool PPCDebugInterface::HasEnabledWatch(u32 address) const
{
  return m_watches.HasEnabledWatch(address);
}

void PPCDebugInterface::RemoveWatch(std::size_t index)
{
  return m_watches.RemoveWatch(index);
}

void PPCDebugInterface::LoadWatchesFromStrings(const std::vector<std::string>& watches)
{
  m_watches.LoadFromStrings(watches);
}

std::vector<std::string> PPCDebugInterface::SaveWatchesToStrings() const
{
  return m_watches.SaveToStrings();
}

void PPCDebugInterface::ClearWatches()
{
  m_watches.Clear();
}

void PPCDebugInterface::SetPatch(u32 address, u32 value)
{
  m_patches.SetPatch(address, value);
}

void PPCDebugInterface::SetPatch(u32 address, std::vector<u8> value)
{
  m_patches.SetPatch(address, value);
}

const std::vector<Common::Debug::MemoryPatch>& PPCDebugInterface::GetPatches() const
{
  return m_patches.GetPatches();
}

void PPCDebugInterface::UnsetPatch(u32 address)
{
  m_patches.UnsetPatch(address);
}

void PPCDebugInterface::EnablePatch(std::size_t index)
{
  m_patches.EnablePatch(index);
}

void PPCDebugInterface::DisablePatch(std::size_t index)
{
  m_patches.DisablePatch(index);
}

bool PPCDebugInterface::HasEnabledPatch(u32 address) const
{
  return m_patches.HasEnabledPatch(address);
}

void PPCDebugInterface::RemovePatch(std::size_t index)
{
  m_patches.RemovePatch(index);
}

void PPCDebugInterface::ClearPatches()
{
  m_patches.ClearPatches();
}

std::string PPCDebugInterface::Disassemble(unsigned int address)
{
  // PowerPC::HostRead_U32 seemed to crash on shutdown
  if (!IsAlive())
    return "";

  if (Core::GetState() == Core::State::Paused)
  {
    if (!PowerPC::HostIsRAMAddress(address))
    {
      return "(No RAM here)";
    }

    const u32 op = PowerPC::HostRead_Instruction(address);
    std::string disasm = Common::GekkoDisassembler::Disassemble(op, address);
    const UGeckoInstruction inst{op};

    if (inst.OPCD == 1)
    {
      disasm += " (hle)";
    }

    return disasm;
  }
  else
  {
    return "<unknown>";
  }
}

std::string PPCDebugInterface::GetRawMemoryString(int memory, unsigned int address)
{
  if (IsAlive())
  {
    const bool is_aram = memory != 0;

    if (is_aram || PowerPC::HostIsRAMAddress(address))
      return StringFromFormat("%08X%s", ReadExtraMemory(memory, address), is_aram ? " (ARAM)" : "");

    return is_aram ? "--ARAM--" : "--------";
  }

  return "<unknwn>";  // bad spelling - 8 chars
}

unsigned int PPCDebugInterface::ReadMemory(unsigned int address)
{
  return PowerPC::HostRead_U32(address);
}

unsigned int PPCDebugInterface::ReadExtraMemory(int memory, unsigned int address)
{
  switch (memory)
  {
  case 0:
    return PowerPC::HostRead_U32(address);
  case 1:
    return (DSP::ReadARAM(address) << 24) | (DSP::ReadARAM(address + 1) << 16) |
           (DSP::ReadARAM(address + 2) << 8) | (DSP::ReadARAM(address + 3));
  default:
    return 0;
  }
}

unsigned int PPCDebugInterface::ReadInstruction(unsigned int address)
{
  return PowerPC::HostRead_Instruction(address);
}

bool PPCDebugInterface::IsAlive()
{
  return Core::IsRunningAndStarted();
}

bool PPCDebugInterface::IsBreakpoint(unsigned int address)
{
  return PowerPC::breakpoints.IsAddressBreakPoint(address);
}

void PPCDebugInterface::SetBreakpoint(unsigned int address)
{
  PowerPC::breakpoints.Add(address);
}

void PPCDebugInterface::ClearBreakpoint(unsigned int address)
{
  PowerPC::breakpoints.Remove(address);
}

void PPCDebugInterface::ClearAllBreakpoints()
{
  PowerPC::breakpoints.Clear();
}

void PPCDebugInterface::ToggleBreakpoint(unsigned int address)
{
  if (PowerPC::breakpoints.IsAddressBreakPoint(address))
    PowerPC::breakpoints.Remove(address);
  else
    PowerPC::breakpoints.Add(address);
}

void PPCDebugInterface::ClearAllMemChecks()
{
  PowerPC::memchecks.Clear();
}

bool PPCDebugInterface::IsMemCheck(unsigned int address, size_t size)
{
  return PowerPC::memchecks.GetMemCheck(address, size) != nullptr;
}

void PPCDebugInterface::ToggleMemCheck(unsigned int address, bool read, bool write, bool log)
{
  if (!IsMemCheck(address))
  {
    // Add Memory Check
    TMemCheck MemCheck;
    MemCheck.start_address = address;
    MemCheck.end_address = address;
    MemCheck.is_break_on_read = read;
    MemCheck.is_break_on_write = write;

    MemCheck.log_on_hit = log;
    MemCheck.break_on_hit = true;

    PowerPC::memchecks.Add(MemCheck);
  }
  else
  {
    PowerPC::memchecks.Remove(address);
  }
}

// =======================================================
// Separate the blocks with colors.
// -------------
int PPCDebugInterface::GetColor(unsigned int address)
{
  if (!IsAlive())
    return 0xFFFFFF;
  if (!PowerPC::HostIsRAMAddress(address))
    return 0xeeeeee;
  static const int colors[6] = {
      0xd0FFFF,  // light cyan
      0xFFd0d0,  // light red
      0xd8d8FF,  // light blue
      0xFFd0FF,  // light purple
      0xd0FFd0,  // light green
      0xFFFFd0,  // light yellow
  };
  Common::Symbol* symbol = g_symbolDB.GetSymbolFromAddr(address);
  if (!symbol)
    return 0xFFFFFF;
  if (symbol->type != Common::Symbol::Type::Function)
    return 0xEEEEFF;
  return colors[symbol->index % 6];
}
// =============

std::string PPCDebugInterface::GetDescription(unsigned int address)
{
  return g_symbolDB.GetDescription(address);
}

unsigned int PPCDebugInterface::GetPC()
{
  return PowerPC::ppcState.pc;
}

void PPCDebugInterface::SetPC(unsigned int address)
{
  PowerPC::ppcState.pc = address;
}

void PPCDebugInterface::RunToBreakpoint()
{
}

void PPCDebugInterface::Clear()
{
  ClearAllBreakpoints();
  ClearAllMemChecks();
  ClearPatches();
  ClearWatches();
}
