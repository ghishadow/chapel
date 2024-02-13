//===----- EHFrameSupportTests.cpp - Unit tests for eh-frame support ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/ExecutionEngine/JITLink/EHFrameSupport.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/MachO_arm64.h"
#include "llvm/Testing/Support/Error.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::jitlink;

// TestObjectBytes contains a MachO arm64 object file with three symbols: foo,
// bar, and main, with corresponding FDEs. It was generated with:
//
// % cat foo.cpp
// extern "C" void e();
// extern "C" void a() {
//   try {
//     e();
//   } catch (int x) {
//   }
// }
// extern "C" void b() noexcept {}
// extern "C" void c() noexcept {}
//
// % clang++ --target=arm64-apple-darwin -femit-dwarf-unwind=always -c -o foo.o
// \
//   foo.c
// % xxd -i foo.o

static const uint8_t TestObjectBytes[] = {
    0xcf, 0xfa, 0xed, 0xfe, 0x0c, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x08, 0x02, 0x00, 0x00,
    0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x19, 0x00, 0x00, 0x00,
    0x88, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x90, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x28, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x90, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x74, 0x65,
    0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5f, 0x5f, 0x54, 0x45, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x02, 0x00, 0x00,
    0x02, 0x00, 0x00, 0x00, 0xb8, 0x03, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x04, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x67, 0x63, 0x63, 0x5f, 0x65, 0x78,
    0x63, 0x65, 0x70, 0x74, 0x5f, 0x74, 0x61, 0x62, 0x5f, 0x5f, 0x54, 0x45,
    0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x98, 0x02, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
    0xd8, 0x03, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5f, 0x5f, 0x63, 0x6f, 0x6d, 0x70, 0x61, 0x63, 0x74, 0x5f, 0x75, 0x6e,
    0x77, 0x69, 0x6e, 0x64, 0x5f, 0x5f, 0x4c, 0x44, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xb0, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0xe0, 0x03, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x65, 0x68,
    0x5f, 0x66, 0x72, 0x61, 0x6d, 0x65, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5f, 0x5f, 0x54, 0x45, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x03, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0xf8, 0x03, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00,
    0x0b, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x32, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
    0x40, 0x04, 0x00, 0x00, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x05, 0x00, 0x00,
    0x88, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x00, 0x50, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
    0x03, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0x83, 0x00, 0xd1, 0xfd, 0x7b, 0x01, 0xa9, 0xfd, 0x43, 0x00, 0x91,
    0x00, 0x00, 0x00, 0x94, 0x01, 0x00, 0x00, 0x14, 0x10, 0x00, 0x00, 0x14,
    0xe8, 0x03, 0x01, 0xaa, 0xe0, 0x07, 0x00, 0xf9, 0xe8, 0x07, 0x00, 0xb9,
    0x01, 0x00, 0x00, 0x14, 0xe8, 0x07, 0x40, 0xb9, 0x08, 0x05, 0x00, 0x71,
    0xe8, 0x07, 0x9f, 0x1a, 0x68, 0x01, 0x00, 0x37, 0x01, 0x00, 0x00, 0x14,
    0xe0, 0x07, 0x40, 0xf9, 0x00, 0x00, 0x00, 0x94, 0x08, 0x00, 0x40, 0xb9,
    0xe8, 0x03, 0x00, 0xb9, 0x00, 0x00, 0x00, 0x94, 0x01, 0x00, 0x00, 0x14,
    0xfd, 0x7b, 0x41, 0xa9, 0xff, 0x83, 0x00, 0x91, 0xc0, 0x03, 0x5f, 0xd6,
    0xe0, 0x07, 0x40, 0xf9, 0x00, 0x00, 0x00, 0x94, 0xc0, 0x03, 0x5f, 0xd6,
    0xc0, 0x03, 0x5f, 0xd6, 0xff, 0x9b, 0x11, 0x01, 0x08, 0x0c, 0x04, 0x18,
    0x01, 0x10, 0x58, 0x00, 0x00, 0x01, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x7a, 0x52, 0x00, 0x01, 0x78, 0x1e, 0x01, 0x10, 0x0c, 0x1f, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xe4, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x34, 0x00, 0x00, 0x00,
    0xc8, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x04, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x7a, 0x50, 0x4c,
    0x52, 0x00, 0x01, 0x78, 0x1e, 0x07, 0x9b, 0x9d, 0xff, 0xff, 0xff, 0x10,
    0x10, 0x0c, 0x1f, 0x00, 0x38, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00,
    0x8c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x68, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x7b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x44, 0x0e, 0x20, 0x48, 0x0c, 0x1d, 0x10, 0x9e, 0x01, 0x9d, 0x02,
    0x0a, 0x02, 0x48, 0x0c, 0x1f, 0x20, 0x48, 0x0e, 0x00, 0xde, 0xdd, 0x44,
    0x0b, 0x00, 0x00, 0x00, 0x64, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x2d,
    0x4c, 0x00, 0x00, 0x00, 0x0b, 0x00, 0x00, 0x2d, 0x40, 0x00, 0x00, 0x00,
    0x0a, 0x00, 0x00, 0x2d, 0x0c, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x2d,
    0x10, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x7d, 0x40, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x06, 0x20, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x06, 0x85, 0x00, 0x00, 0x00,
    0x04, 0x00, 0x00, 0x1e, 0x85, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x0e,
    0x74, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1e, 0x74, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x0e, 0x63, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x7d,
    0x38, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1e, 0x38, 0x00, 0x00, 0x00,
    0x07, 0x00, 0x00, 0x0e, 0x1c, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x1e,
    0x1c, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x0e, 0x70, 0x00, 0x00, 0x00,
    0x0e, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x54, 0x00, 0x00, 0x00, 0x0e, 0x02, 0x00, 0x00, 0x70, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x76, 0x00, 0x00, 0x00, 0x0e, 0x02, 0x00, 0x00,
    0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4e, 0x00, 0x00, 0x00,
    0x0e, 0x03, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x48, 0x00, 0x00, 0x00, 0x0e, 0x04, 0x00, 0x00, 0xe8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x42, 0x00, 0x00, 0x00,
    0x0f, 0x01, 0x00, 0x00, 0x68, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3f, 0x00, 0x00, 0x00, 0x0f, 0x01, 0x00, 0x00, 0x6c, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x2c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x08, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x1b, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x5a, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x3c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x5f, 0x5f, 0x5a, 0x54, 0x49, 0x69, 0x00,
    0x5f, 0x5f, 0x5f, 0x63, 0x78, 0x61, 0x5f, 0x62, 0x65, 0x67, 0x69, 0x6e,
    0x5f, 0x63, 0x61, 0x74, 0x63, 0x68, 0x00, 0x5f, 0x5f, 0x5f, 0x63, 0x78,
    0x61, 0x5f, 0x65, 0x6e, 0x64, 0x5f, 0x63, 0x61, 0x74, 0x63, 0x68, 0x00,
    0x5f, 0x5f, 0x55, 0x6e, 0x77, 0x69, 0x6e, 0x64, 0x5f, 0x52, 0x65, 0x73,
    0x75, 0x6d, 0x65, 0x00, 0x5f, 0x65, 0x00, 0x5f, 0x63, 0x00, 0x5f, 0x62,
    0x00, 0x5f, 0x61, 0x00, 0x6c, 0x74, 0x6d, 0x70, 0x33, 0x00, 0x6c, 0x74,
    0x6d, 0x70, 0x32, 0x00, 0x6c, 0x74, 0x6d, 0x70, 0x31, 0x00, 0x5f, 0x5f,
    0x5f, 0x67, 0x78, 0x78, 0x5f, 0x70, 0x65, 0x72, 0x73, 0x6f, 0x6e, 0x61,
    0x6c, 0x69, 0x74, 0x79, 0x5f, 0x76, 0x30, 0x00, 0x6c, 0x74, 0x6d, 0x70,
    0x30, 0x00, 0x47, 0x43, 0x43, 0x5f, 0x65, 0x78, 0x63, 0x65, 0x70, 0x74,
    0x5f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x30, 0x00};

llvm::MemoryBufferRef
    TestObject(StringRef(reinterpret_cast<const char *>(TestObjectBytes),
                         sizeof(TestObjectBytes)),
               "foo.o");

TEST(EHFrameCFIBlockInspector, BasicSuccessCase) {
  // Create a LinkGraph from the test object above and verify that
  // (1) There are two CIEs -- one with a personality function and one
  //     without.
  // (2) There are three FDEs -- two attached to the CIE with no
  //     personality, one attached to the CIE with a personality.
  // (3) Each FDE has an edge pointing to the CIE at the correct offset.
  // (4) Each function has exactly one FDE pointing at it.

  auto G = cantFail(createLinkGraphFromMachOObject_arm64(TestObject));
  cantFail(createEHFrameSplitterPass_MachO_arm64()(*G));
  cantFail(createEHFrameEdgeFixerPass_MachO_arm64()(*G));

  auto *EHFrame = G->findSectionByName("__TEXT,__eh_frame");
  assert(EHFrame && "eh-frame missing?");

  SmallVector<Block *, 2> CIEs;
  for (auto *B : EHFrame->blocks()) {
    auto CFIBI = EHFrameCFIBlockInspector::FromEdgeScan(*B);
    if (CFIBI.isCIE()) {
      CIEs.push_back(B);
      // If this CIE has an edge, check that getPersonalityEdge returns it.
      if (B->edges_size() != 0)
        EXPECT_TRUE(!!CFIBI.getPersonalityEdge());
    }
  }
  ASSERT_EQ(CIEs.size(), 2U);

  // Make sure that the CIE with no edges is CIEs[0].
  if (CIEs[1]->edges_empty())
    std::swap(CIEs[0], CIEs[1]);

  EXPECT_TRUE(CIEs[0]->edges_empty());
  EXPECT_EQ(CIEs[1]->edges_size(), 1U);

  std::set<StringRef> Targets;
  for (auto *B : EHFrame->blocks()) {
    auto CFIBI = EHFrameCFIBlockInspector::FromEdgeScan(*B);
    if (CFIBI.isFDE()) {
      ASSERT_TRUE(!!CFIBI.getCIEEdge());
      ASSERT_TRUE(CFIBI.getCIEEdge()->getTarget().isDefined());
      auto &CIE = CFIBI.getCIEEdge()->getTarget().getBlock();
      ASSERT_TRUE(&CIE == CIEs[0] || &CIE == CIEs[1]);

      ASSERT_TRUE(!!CFIBI.getPCBeginEdge());
      auto &PCBeginTarget = CFIBI.getPCBeginEdge()->getTarget();
      ASSERT_TRUE(PCBeginTarget.hasName());
      Targets.insert(PCBeginTarget.getName());

      // If the FDE points at CIEs[0] (the CIE without a personality) then it
      // should not have an LSDA. If it points to CIEs[1] then it should have
      // an LSDA.
      if (&CIE == CIEs[0])
        EXPECT_EQ(CFIBI.getLSDAEdge(), nullptr);
      else
        EXPECT_NE(CFIBI.getLSDAEdge(), nullptr);
    }
  }

  EXPECT_EQ(Targets.size(), 3U) << "Unexpected number of FDEs";
  EXPECT_EQ(Targets.count("_a"), 1U);
  EXPECT_EQ(Targets.count("_b"), 1U);
  EXPECT_EQ(Targets.count("_c"), 1U);
}

TEST(EHFrameCFIBlockInspector, ExternalPCBegin) {
  // Check that we don't crash if we transform the target of an FDE into an
  // external symbol before running edge-fixing.
  auto G = cantFail(createLinkGraphFromMachOObject_arm64(TestObject));

  // Make '_a' external.
  for (auto *Sym : G->defined_symbols())
    if (Sym->hasName() && Sym->getName() == "_a") {
      G->makeExternal(*Sym);
      break;
    }

  // Run the splitter and edge-fixer passes.
  cantFail(createEHFrameSplitterPass_MachO_arm64()(*G));
  cantFail(createEHFrameEdgeFixerPass_MachO_arm64()(*G));
}
