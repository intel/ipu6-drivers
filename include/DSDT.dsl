/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20180810 (64-bit version)
 * Copyright (c) 2000 - 2018 Intel Corporation
 * 
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of DSDT.asm, Fri May 29 01:10:10 2020
 *
 * Original Table Header:
 *     Signature        "DSDT"
 *     Length           0x0000315A (12634)
 *     Revision         0x02
 *     Checksum         0xDE
 *     OEM ID           "COREv4"
 *     OEM Table ID     "COREBOOT"
 *     OEM Revision     0x20110725 (537986853)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20190703 (538511107)
 */
DefinitionBlock ("", "DSDT", 2, "COREv4", "COREBOOT", 0x20110725)
{
    External (_PR_.CNOT, MethodObj)    // 1 Arguments
    External (_SB_.MPTS, MethodObj)    // 1 Arguments
    External (_SB_.MWAK, MethodObj)    // 1 Arguments
    External (NVSA, UnknownObj)
    External (OIPG, UnknownObj)

    Scope (\)
    {
        Name (OIPG, Package (0x01)
        {
            Package (0x04)
            {
                One, 
                Zero, 
                0xFFFFFFFFFFFFFFFF, 
                "INT34C5:00"
            }
        })
    }

    Scope (\)
    {
        Name (NVSA, 0x5BAFC000)
    }

    Scope (_SB)
    {
        Method (_SWS, 0, NotSerialized)  // _SWS: System Wake Source
        {
            Return (PM1I) /* \PM1I */
        }
    }

    Scope (_GPE)
    {
        Method (_SWS, 0, NotSerialized)  // _SWS: System Wake Source
        {
            Return (GPEI) /* \GPEI */
        }
    }

    OperationRegion (POST, SystemIO, 0x80, One)
    Field (POST, ByteAcc, Lock, Preserve)
    {
        DBG0,   8
    }

    Method (_PTS, 1, NotSerialized)  // _PTS: Prepare To Sleep
    {
        DBG0 = 0x96
        If (CondRefOf (\_SB.MPTS))
        {
            \_SB.MPTS (Arg0)
        }
    }

    Method (_WAK, 1, NotSerialized)  // _WAK: Wake
    {
        DBG0 = 0x97
        If (CondRefOf (\_SB.MWAK))
        {
            \_SB.MWAK (Arg0)
        }

        Return (Package (0x02)
        {
            Zero, 
            Zero
        })
    }

    OperationRegion (APMP, SystemIO, 0xB2, 0x02)
    Field (APMP, ByteAcc, NoLock, Preserve)
    {
        APMC,   8, 
        APMS,   8
    }

    Method (_PIC, 1, NotSerialized)  // _PIC: Interrupt Model
    {
        PICM = Arg0
    }

    Name (PICM, Zero)
    OperationRegion (GNVS, SystemMemory, NVSA, 0x2000)
    Field (GNVS, ByteAcc, NoLock, Preserve)
    {
        OSYS,   16, 
        SMIF,   8, 
        PCNT,   8, 
        PPCM,   8, 
        TLVL,   8, 
        LIDS,   8, 
        PWRS,   8, 
        CBMC,   32, 
        PM1I,   64, 
        GPEI,   64, 
        DPTE,   8, 
        NHLA,   64, 
        NHLL,   32, 
        CID1,   16, 
        U2WE,   16, 
        U3WE,   16, 
        UIOR,   8, 
        S5U0,   8, 
        S3U0,   8, 
        Offset (0x100), 
        VBT0,   32, 
        VBT1,   32, 
        VBT2,   32, 
        VBT3,   16, 
        VBT4,   2048, 
        VBT5,   512, 
        VBT6,   512, 
        VBT7,   32, 
        VBT8,   32, 
        VBT9,   32, 
        CHVD,   24576, 
        VBTA,   32, 
        MEHH,   256, 
        RMOB,   32, 
        RMOL,   32, 
        ROVP,   32, 
        ROVL,   32, 
        RWVP,   32, 
        RWVL,   32
    }

    Method (S3UE, 0, NotSerialized)
    {
        S3U0 = One
    }

    Method (S3UD, 0, NotSerialized)
    {
        S3U0 = Zero
    }

    Method (S5UE, 0, NotSerialized)
    {
        S5U0 = One
    }

    Method (S5UD, 0, NotSerialized)
    {
        S5U0 = Zero
    }

    Method (PNOT, 0, NotSerialized)
    {
        \_PR.CNOT (0x81)
    }

    Method (PPCN, 0, NotSerialized)
    {
        \_PR.CNOT (0x80)
    }

    Method (TNOT, 0, NotSerialized)
    {
        \_PR.CNOT (0x82)
    }

    Scope (_SB)
    {
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A08") /* PCI Express Bus */)  // _HID: Hardware ID
            Name (_CID, EisaId ("PNP0A03") /* PCI Bus */)  // _CID: Compatible ID
            Name (_SEG, Zero)  // _SEG: PCI Segment
            Name (_ADR, Zero)  // _ADR: Address
            Name (_UID, Zero)  // _UID: Unique ID
            Device (MCHC)
            {
                Name (_ADR, Zero)  // _ADR: Address
                OperationRegion (MCHP, PCI_Config, Zero, 0x0100)
                Field (MCHP, DWordAcc, NoLock, Preserve)
                {
                    Offset (0x40), 
                    EPEN,   1, 
                        ,   11, 
                    EPBR,   20, 
                    Offset (0x48), 
                    MHEN,   1, 
                        ,   14, 
                    MHBR,   17, 
                    Offset (0x60), 
                    PXEN,   1, 
                    PXSZ,   2, 
                        ,   23, 
                    PXBR,   6, 
                    Offset (0x68), 
                    DIEN,   1, 
                        ,   11, 
                    DIBR,   20, 
                    Offset (0xA0), 
                    TOM,    64, 
                    TUUD,   64, 
                    Offset (0xBC), 
                    TLUD,   32
                }
            }

            Name (MCRS, ResourceTemplate ()
            {
                WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                    0x0000,             // Granularity
                    0x0000,             // Range Minimum
                    0x00FF,             // Range Maximum
                    0x0000,             // Translation Offset
                    0x0100,             // Length
                    ,, )
                DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x00000000,         // Granularity
                    0x00000000,         // Range Minimum
                    0x00000CF7,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00000CF8,         // Length
                    ,, , TypeStatic, DenseTranslation)
                IO (Decode16,
                    0x0CF8,             // Range Minimum
                    0x0CF8,             // Range Maximum
                    0x01,               // Alignment
                    0x08,               // Length
                    )
                DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                    0x00000000,         // Granularity
                    0x00000D00,         // Range Minimum
                    0x0000FFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x0000F300,         // Length
                    ,, , TypeStatic, DenseTranslation)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000A0000,         // Range Minimum
                    0x000BFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00020000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000C0000,         // Range Minimum
                    0x000C3FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000C4000,         // Range Minimum
                    0x000C7FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000C8000,         // Range Minimum
                    0x000CBFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000CC000,         // Range Minimum
                    0x000CFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000D0000,         // Range Minimum
                    0x000D3FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000D4000,         // Range Minimum
                    0x000D7FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000D8000,         // Range Minimum
                    0x000DBFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000DC000,         // Range Minimum
                    0x000DFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000E0000,         // Range Minimum
                    0x000E3FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000E4000,         // Range Minimum
                    0x000E7FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000E8000,         // Range Minimum
                    0x000EBFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000EC000,         // Range Minimum
                    0x000EFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00004000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x000F0000,         // Range Minimum
                    0x000FFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00010000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0x00000000,         // Range Minimum
                    0xDFFFFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0xE0000000,         // Length
                    ,, _Y00, AddressRangeMemory, TypeStatic)
                QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                    0x0000000000000000, // Granularity
                    0x0000000000010000, // Range Minimum
                    0x000000000001FFFF, // Range Maximum
                    0x0000000000000000, // Translation Offset
                    0x0000000000010000, // Length
                    ,, _Y01, AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0xFC800000,         // Range Minimum
                    0xFE7FFFFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x02000000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
                DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, Cacheable, ReadWrite,
                    0x00000000,         // Granularity
                    0xFED40000,         // Range Minimum
                    0xFED47FFF,         // Range Maximum
                    0x00000000,         // Translation Offset
                    0x00008000,         // Length
                    ,, , AddressRangeMemory, TypeStatic)
            })
            Method (_CRS, 0, Serialized)  // _CRS: Current Resource Settings
            {
                CreateDWordField (MCRS, \_SB.PCI0._Y00._MIN, PMIN)  // _MIN: Minimum Base Address
                CreateDWordField (MCRS, \_SB.PCI0._Y00._MAX, PMAX)  // _MAX: Maximum Base Address
                CreateDWordField (MCRS, \_SB.PCI0._Y00._LEN, PLEN)  // _LEN: Length
                PMIN = ^MCHC.TLUD /* \_SB_.PCI0.MCHC.TLUD */
                PLEN = ((PMAX - PMIN) + One)
                CreateQWordField (MCRS, \_SB.PCI0._Y01._MIN, MMIN)  // _MIN: Minimum Base Address
                CreateQWordField (MCRS, \_SB.PCI0._Y01._MAX, MMAX)  // _MAX: Maximum Base Address
                CreateQWordField (MCRS, \_SB.PCI0._Y01._LEN, MLEN)  // _LEN: Length
                Local0 = ^MCHC.TUUD /* \_SB_.PCI0.MCHC.TUUD */
                If ((Local0 <= 0x0000000800000000))
                {
                    MMIN = 0x0000000800000000
                    MLEN = 0x0000000400000000
                }
                Else
                {
                    MMIN = Zero
                    MLEN = Zero
                }

                MMAX = ((MMIN + MLEN) - One)
                Return (MCRS) /* \_SB_.PCI0.MCRS */
            }

            Name (EP_B, Zero)
            Name (MH_B, Zero)
            Name (PC_B, Zero)
            Name (PC_L, Zero)
            Name (DM_B, Zero)
            Method (GMHB, 0, Serialized)
            {
                If ((MH_B == Zero))
                {
                    MH_B = (^MCHC.MHBR << 0x0F)
                }

                Return (MH_B) /* \_SB_.PCI0.MH_B */
            }

            Method (GEPB, 0, Serialized)
            {
                If ((EP_B == Zero))
                {
                    EP_B = (^MCHC.EPBR << 0x0C)
                }

                Return (EP_B) /* \_SB_.PCI0.EP_B */
            }

            Method (GPCB, 0, Serialized)
            {
                If ((PC_B == Zero))
                {
                    PC_B = (^MCHC.PXBR << 0x1A)
                }

                Return (PC_B) /* \_SB_.PCI0.PC_B */
            }

            Method (GPCL, 0, Serialized)
            {
                If ((PC_L == Zero))
                {
                    PC_L = (0x10000000 >> ^MCHC.PXSZ) /* \_SB_.PCI0.MCHC.PXSZ */
                }

                Return (PC_L) /* \_SB_.PCI0.PC_L */
            }

            Method (GDMB, 0, Serialized)
            {
                If ((DM_B == Zero))
                {
                    DM_B = (^MCHC.DIBR << 0x0C)
                }

                Return (DM_B) /* \_SB_.PCI0.DM_B */
            }

            Device (PDRC)
            {
                Name (_HID, EisaId ("PNP0C02") /* PNP Motherboard Resources */)  // _HID: Hardware ID
                Name (_UID, One)  // _UID: Unique ID
                Name (BUF0, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00008000,         // Address Length
                        _Y02)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00001000,         // Address Length
                        _Y03)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00001000,         // Address Length
                        _Y04)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000000,         // Address Length
                        _Y05)
                    Memory32Fixed (ReadOnly,
                        0xFED90000,         // Address Base
                        0x00004000,         // Address Length
                        )
                    Memory32Fixed (ReadOnly,
                        0xFFF00000,         // Address Base
                        0x01000000,         // Address Length
                        )
                    Memory32Fixed (ReadOnly,
                        0xFEE00000,         // Address Base
                        0x00100000,         // Address Length
                        )
                    Memory32Fixed (ReadWrite,
                        0xFED00000,         // Address Base
                        0x00000400,         // Address Length
                        )
                })
                Method (_CRS, 0, Serialized)  // _CRS: Current Resource Settings
                {
                    CreateDWordField (BUF0, \_SB.PCI0.PDRC._Y02._BAS, MBR0)  // _BAS: Base Address
                    MBR0 = GMHB ()
                    CreateDWordField (BUF0, \_SB.PCI0.PDRC._Y03._BAS, DBR0)  // _BAS: Base Address
                    DBR0 = GDMB ()
                    CreateDWordField (BUF0, \_SB.PCI0.PDRC._Y04._BAS, EBR0)  // _BAS: Base Address
                    EBR0 = GEPB ()
                    CreateDWordField (BUF0, \_SB.PCI0.PDRC._Y05._BAS, XBR0)  // _BAS: Base Address
                    XBR0 = GPCB ()
                    CreateDWordField (BUF0, \_SB.PCI0.PDRC._Y05._LEN, XSZ0)  // _LEN: Length
                    XSZ0 = GPCL ()
                    Return (BUF0) /* \_SB_.PCI0.PDRC.BUF0 */
                }
            }

            Scope (\_SB)
            {
                Name (SBRG, 0xFD000000)
                Name (ICKP, 0xAD)
                OperationRegion (ICLK, SystemMemory, (SBRG + ((ICKP << 0x10) + 0x8000)), 0x40)
                Field (ICLK, AnyAcc, Lock, Preserve)
                {
                    CLK1,   8, 
                    Offset (0x0C), 
                    CLK2,   8, 
                    Offset (0x18), 
                    CLK3,   8, 
                    Offset (0x24), 
                    CLK4,   8, 
                    Offset (0x30), 
                    CLK5,   8, 
                    Offset (0x3C), 
                    CLK6,   8
                }

                Method (NCLK, 0, NotSerialized)
                {
                    Return (0x06)
                }

                Method (CLKC, 2, Serialized)
                {
                    Switch (ToInteger (Arg0))
                    {
                        Case (Zero)
                        {
                            Local0 = CLK1 /* \_SB_.CLK1 */
                            CLK1 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }
                        Case (One)
                        {
                            Local0 = CLK2 /* \_SB_.CLK2 */
                            CLK2 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }
                        Case (0x02)
                        {
                            Local0 = CLK3 /* \_SB_.CLK3 */
                            CLK3 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }
                        Case (0x03)
                        {
                            Local0 = CLK4 /* \_SB_.CLK4 */
                            CLK4 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }
                        Case (0x04)
                        {
                            Local0 = CLK5 /* \_SB_.CLK5 */
                            CLK5 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }
                        Case (0x05)
                        {
                            Local0 = CLK6 /* \_SB_.CLK6 */
                            CLK6 = ((Local0 & 0xFFFFFFFFFFFFFFFD) | (Arg1 << One))
                        }

                    }
                }

                Method (CLKF, 2, Serialized)
                {
                    Switch (ToInteger (Arg0))
                    {
                        Case (Zero)
                        {
                            Local0 = CLK1 /* \_SB_.CLK1 */
                            CLK1 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }
                        Case (One)
                        {
                            Local0 = CLK2 /* \_SB_.CLK2 */
                            CLK2 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }
                        Case (0x02)
                        {
                            Local0 = CLK3 /* \_SB_.CLK3 */
                            CLK3 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }
                        Case (0x03)
                        {
                            Local0 = CLK4 /* \_SB_.CLK4 */
                            CLK4 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }
                        Case (0x04)
                        {
                            Local0 = CLK5 /* \_SB_.CLK5 */
                            CLK5 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }
                        Case (0x05)
                        {
                            Local0 = CLK6 /* \_SB_.CLK6 */
                            CLK6 = ((Local0 & 0xFFFFFFFFFFFFFFFE) | Arg1)
                        }

                    }
                }

                Method (MCCT, 3, NotSerialized)
                {
                    CLKC (ToInteger (Arg0), ToInteger (Arg1))
                    CLKF (ToInteger (Arg0), ToInteger (Arg2))
                }
            }

            Name (PICP, Package (0x3A)
            {
                Package (0x04)
                {
                    0x001FFFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x001EFFFF, 
                    Zero, 
                    Zero, 
                    0x22
                }, 

                Package (0x04)
                {
                    0x001EFFFF, 
                    One, 
                    Zero, 
                    0x23
                }, 

                Package (0x04)
                {
                    0x001EFFFF, 
                    0x02, 
                    Zero, 
                    0x24
                }, 

                Package (0x04)
                {
                    0x001EFFFF, 
                    0x03, 
                    Zero, 
                    0x25
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x0019FFFF, 
                    Zero, 
                    Zero, 
                    0x1F
                }, 

                Package (0x04)
                {
                    0x0019FFFF, 
                    One, 
                    Zero, 
                    0x20
                }, 

                Package (0x04)
                {
                    0x0019FFFF, 
                    0x02, 
                    Zero, 
                    0x21
                }, 

                Package (0x04)
                {
                    0x0017FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x0015FFFF, 
                    Zero, 
                    Zero, 
                    0x1B
                }, 

                Package (0x04)
                {
                    0x0015FFFF, 
                    One, 
                    Zero, 
                    0x1C
                }, 

                Package (0x04)
                {
                    0x0015FFFF, 
                    0x02, 
                    Zero, 
                    0x1D
                }, 

                Package (0x04)
                {
                    0x0015FFFF, 
                    0x03, 
                    Zero, 
                    0x1E
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x0013FFFF, 
                    Zero, 
                    Zero, 
                    0x17
                }, 

                Package (0x04)
                {
                    0x0013FFFF, 
                    One, 
                    Zero, 
                    0x18
                }, 

                Package (0x04)
                {
                    0x0013FFFF, 
                    0x02, 
                    Zero, 
                    0x19
                }, 

                Package (0x04)
                {
                    0x0013FFFF, 
                    0x03, 
                    Zero, 
                    0x26
                }, 

                Package (0x04)
                {
                    0x0012FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0011FFFF, 
                    Zero, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x0011FFFF, 
                    One, 
                    Zero, 
                    0x14
                }, 

                Package (0x04)
                {
                    0x0011FFFF, 
                    0x02, 
                    Zero, 
                    0x15
                }, 

                Package (0x04)
                {
                    0x0011FFFF, 
                    0x03, 
                    Zero, 
                    0x16
                }, 

                Package (0x04)
                {
                    0x0010FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0010FFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x0010FFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x0002FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0004FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0005FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0008FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    One, 
                    Zero, 
                    0x11
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x02, 
                    Zero, 
                    0x12
                }, 

                Package (0x04)
                {
                    0x0007FFFF, 
                    0x03, 
                    Zero, 
                    0x13
                }, 

                Package (0x04)
                {
                    0x000DFFFF, 
                    Zero, 
                    Zero, 
                    0x10
                }, 

                Package (0x04)
                {
                    0x000DFFFF, 
                    One, 
                    Zero, 
                    0x11
                }
            })
            Name (PICN, Package (0x1D)
            {
                Package (0x04)
                {
                    0x001FFFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001FFFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001DFFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x001CFFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0017FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0016FFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0014FFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    One, 
                    Zero, 
                    0x0A
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x02, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0001FFFF, 
                    0x03, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0002FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0004FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0005FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }, 

                Package (0x04)
                {
                    0x0008FFFF, 
                    Zero, 
                    Zero, 
                    0x0B
                }
            })
            Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
            {
                If (PICM)
                {
                    Return (PICP) /* \_SB_.PCI0.PICP */
                }
                Else
                {
                    Return (PICN) /* \_SB_.PCI0.PICN */
                }
            }

            Method (PCRB, 1, NotSerialized)
            {
                Return ((0xFD000000 + (Arg0 << 0x10)))
            }

            Method (PCRR, 2, Serialized)
            {
                OperationRegion (PCRD, SystemMemory, (PCRB (Arg0) + Arg1), 0x04)
                Field (PCRD, DWordAcc, NoLock, Preserve)
                {
                    DATA,   32
                }

                Return (DATA) /* \_SB_.PCI0.PCRR.DATA */
            }

            Method (PCRA, 3, Serialized)
            {
                OperationRegion (PCRD, SystemMemory, (PCRB (Arg0) + Arg1), 0x04)
                Field (PCRD, DWordAcc, NoLock, Preserve)
                {
                    DATA,   32
                }

                DATA &= Arg2
                PCRR (Arg0, Arg1)
            }

            Method (PCRO, 3, Serialized)
            {
                OperationRegion (PCRD, SystemMemory, (PCRB (Arg0) + Arg1), 0x04)
                Field (PCRD, DWordAcc, NoLock, Preserve)
                {
                    DATA,   32
                }

                DATA |= Arg2
                PCRR (Arg0, Arg1)
            }

            Scope (\_SB.PCI0)
            {
                Device (PEMC)
                {
                    Name (_ADR, 0x001A0000)  // _ADR: Address
                    Name (_DDN, "eMMC Controller")  // _DDN: DOS Device Name
                    Name (TEMP, Zero)
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (0x0F)
                    }

                    OperationRegion (SCSR, PCI_Config, Zero, 0x0100)
                    Field (SCSR, WordAcc, NoLock, Preserve)
                    {
                        Offset (0x84), 
                        PSTA,   32, 
                        Offset (0xA2), 
                            ,   2, 
                        PGEN,   1
                    }

                    Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                    {
                        PGEN = Zero
                        PSTA &= 0xFFFFFFFC
                        TEMP = PSTA /* \_SB_.PCI0.PEMC.PSTA */
                    }

                    Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                    {
                        PGEN = One
                        PSTA |= 0x03
                        TEMP = PSTA /* \_SB_.PCI0.PEMC.PSTA */
                    }

                    Device (CARD)
                    {
                        Name (_ADR, 0x08)  // _ADR: Address
                        Method (_RMV, 0, NotSerialized)  // _RMV: Removal Status
                        {
                            Return (Zero)
                        }
                    }
                }

                Device (SDXC)
                {
                    Name (_ADR, 0x00140005)  // _ADR: Address
                    Name (_DDN, "SD Controller")  // _DDN: DOS Device Name
                    Name (TEMP, Zero)
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (0x0F)
                    }

                    OperationRegion (SCSR, PCI_Config, Zero, 0x0100)
                    Field (SCSR, WordAcc, NoLock, Preserve)
                    {
                        Offset (0x84), 
                        PSTA,   32, 
                        Offset (0xA2), 
                            ,   2, 
                        PGEN,   1
                    }

                    Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                    {
                        PGEN = Zero
                        PSTA &= 0xFFFFFFFC
                        TEMP = PSTA /* \_SB_.PCI0.SDXC.PSTA */
                    }

                    Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                    {
                        PGEN = One
                        PSTA |= 0x03
                        TEMP = PSTA /* \_SB_.PCI0.SDXC.PSTA */
                    }

                    Device (CARD)
                    {
                        Name (_ADR, 0x08)  // _ADR: Address
                        Method (_RMV, 0, NotSerialized)  // _RMV: Removal Status
                        {
                            Return (One)
                        }
                    }
                }
            }

            Device (GPIO)
            {
                Name (_HID, "INT34C5")  // _HID: Hardware ID
                Name (_UID, Zero)  // _UID: Unique ID
                Name (_DDN, "GPIO Controller")  // _DDN: DOS Device Name
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000000,         // Address Length
                        _Y06)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000000,         // Address Length
                        _Y07)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000000,         // Address Length
                        _Y08)
                    Memory32Fixed (ReadWrite,
                        0x00000000,         // Address Base
                        0x00000000,         // Address Length
                        _Y09)
                    Interrupt (ResourceConsumer, Level, ActiveLow, Shared, ,, )
                    {
                        0x0000000E,
                    }
                })
                Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                {
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y06._BAS, BAS0)  // _BAS: Base Address
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y06._LEN, LEN0)  // _LEN: Length
                    BAS0 = PCRB (0x6E)
                    LEN0 = 0x00010000
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y07._BAS, BAS1)  // _BAS: Base Address
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y07._LEN, LEN1)  // _LEN: Length
                    BAS1 = PCRB (0x6D)
                    LEN1 = 0x00010000
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y08._BAS, BAS4)  // _BAS: Base Address
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y08._LEN, LEN4)  // _LEN: Length
                    BAS4 = PCRB (0x6A)
                    LEN4 = 0x00010000
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y09._BAS, BAS5)  // _BAS: Base Address
                    CreateDWordField (RBUF, \_SB.PCI0.GPIO._Y09._LEN, LEN5)  // _LEN: Length
                    BAS5 = PCRB (0x69)
                    LEN5 = 0x00010000
                    Return (RBUF) /* \_SB_.PCI0.GPIO.RBUF */
                }

                Method (_STA, 0, NotSerialized)  // _STA: Status
                {
                    Return (0x0F)
                }
            }

            Method (GADD, 1, NotSerialized)
            {
                If (((Arg0 >= Zero) && (Arg0 <= 0x42)))
                {
                    Local0 = 0x6E
                    Local1 = (Arg0 - Zero)
                }

                If (((Arg0 >= 0x43) && (Arg0 <= 0xAA)))
                {
                    Local0 = 0x6D
                    Local1 = (Arg0 - 0x43)
                }

                If (((Arg0 >= 0xBC) && (Arg0 <= 0x0115)))
                {
                    Local0 = 0x6A
                    Local1 = (Arg0 - 0xBC)
                }

                If (((Arg0 >= 0x0116) && (Arg0 <= 0x0126)))
                {
                    Local0 = 0x69
                    Local1 = (Arg0 - 0x0116)
                }

                Local2 = PCRB (Local0)
                Local2 += 0x0700
                Return ((Local2 + (Local1 * 0x10)))
            }

            Method (GRXS, 1, Serialized)
            {
                OperationRegion (PREG, SystemMemory, GADD (Arg0), 0x04)
                Field (PREG, AnyAcc, NoLock, Preserve)
                {
                    VAL0,   32
                }

                Local0 = (One & (VAL0 >> One))
                Return (Local0)
            }

            Method (GTXS, 1, Serialized)
            {
                OperationRegion (PREG, SystemMemory, GADD (Arg0), 0x04)
                Field (PREG, AnyAcc, NoLock, Preserve)
                {
                    VAL0,   32
                }

                Local0 = (One & VAL0) /* \_SB_.PCI0.GTXS.VAL0 */
                Return (Local0)
            }

            Method (STXS, 1, Serialized)
            {
                OperationRegion (PREG, SystemMemory, GADD (Arg0), 0x04)
                Field (PREG, AnyAcc, NoLock, Preserve)
                {
                    VAL0,   32
                }

                VAL0 |= One /* \_SB_.PCI0.STXS.VAL0 */
            }

            Method (CTXS, 1, Serialized)
            {
                OperationRegion (PREG, SystemMemory, GADD (Arg0), 0x04)
                Field (PREG, AnyAcc, NoLock, Preserve)
                {
                    VAL0,   32
                }

                VAL0 &= 0xFFFFFFFFFFFFFFFE /* \_SB_.PCI0.CTXS.VAL0 */
            }

            Device (LPCB)
            {
                Name (_ADR, 0x001F0000)  // _ADR: Address
                Name (_DDN, "ESPI Bus Device")  // _DDN: DOS Device Name
                Device (FWH)
                {
                    Name (_HID, EisaId ("INT0800") /* Intel 82802 Firmware Hub Device */)  // _HID: Hardware ID
                    Name (_DDN, "Firmware Hub")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        Memory32Fixed (ReadOnly,
                            0xFF000000,         // Address Base
                            0x01000000,         // Address Length
                            )
                    })
                }

                Device (HPET)
                {
                    Name (_HID, EisaId ("PNP0103") /* HPET System Timer */)  // _HID: Hardware ID
                    Name (_DDN, "High Precision Event Timer")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        Memory32Fixed (ReadWrite,
                            0xFED00000,         // Address Base
                            0x00000400,         // Address Length
                            )
                    })
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (0x0F)
                    }
                }

                Device (PIC)
                {
                    Name (_HID, EisaId ("PNP0000") /* 8259-compatible Programmable Interrupt Controller */)  // _HID: Hardware ID
                    Name (_DDN, "8259 Interrupt Controller")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0020,             // Range Minimum
                            0x0020,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0024,             // Range Minimum
                            0x0024,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0028,             // Range Minimum
                            0x0028,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x002C,             // Range Minimum
                            0x002C,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0030,             // Range Minimum
                            0x0030,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0034,             // Range Minimum
                            0x0034,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0038,             // Range Minimum
                            0x0038,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x003C,             // Range Minimum
                            0x003C,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A0,             // Range Minimum
                            0x00A0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A4,             // Range Minimum
                            0x00A4,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00A8,             // Range Minimum
                            0x00A8,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00AC,             // Range Minimum
                            0x00AC,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00B0,             // Range Minimum
                            0x00B0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00B4,             // Range Minimum
                            0x00B4,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00B8,             // Range Minimum
                            0x00B8,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x00BC,             // Range Minimum
                            0x00BC,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x04D0,             // Range Minimum
                            0x04D0,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IRQNoFlags ()
                            {2}
                    })
                }

                Device (LDRC)
                {
                    Name (_HID, EisaId ("PNP0C02") /* PNP Motherboard Resources */)  // _HID: Hardware ID
                    Name (_UID, 0x02)  // _UID: Unique ID
                    Name (_DDN, "Legacy Device Resources")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x002E,             // Range Minimum
                            0x002E,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x004E,             // Range Minimum
                            0x004E,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x0061,             // Range Minimum
                            0x0061,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0063,             // Range Minimum
                            0x0063,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0065,             // Range Minimum
                            0x0065,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0067,             // Range Minimum
                            0x0067,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0080,             // Range Minimum
                            0x0080,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x0092,             // Range Minimum
                            0x0092,             // Range Maximum
                            0x01,               // Alignment
                            0x01,               // Length
                            )
                        IO (Decode16,
                            0x00B2,             // Range Minimum
                            0x00B2,             // Range Maximum
                            0x01,               // Alignment
                            0x02,               // Length
                            )
                        IO (Decode16,
                            0x1800,             // Range Minimum
                            0x1800,             // Range Maximum
                            0x01,               // Alignment
                            0xFF,               // Length
                            )
                    })
                }

                Device (RTC)
                {
                    Name (_HID, EisaId ("PNP0B00") /* AT Real-Time Clock */)  // _HID: Hardware ID
                    Name (_DDN, "Real Time Clock")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0070,             // Range Minimum
                            0x0070,             // Range Maximum
                            0x01,               // Alignment
                            0x08,               // Length
                            )
                    })
                }

                Device (TIMR)
                {
                    Name (_HID, EisaId ("PNP0100") /* PC-class System Timer */)  // _HID: Hardware ID
                    Name (_DDN, "8254 Timer")  // _DDN: DOS Device Name
                    Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
                    {
                        IO (Decode16,
                            0x0040,             // Range Minimum
                            0x0040,             // Range Maximum
                            0x01,               // Alignment
                            0x04,               // Length
                            )
                        IO (Decode16,
                            0x0050,             // Range Minimum
                            0x0050,             // Range Maximum
                            0x10,               // Alignment
                            0x04,               // Length
                            )
                        IRQNoFlags ()
                            {0}
                    })
                }
            }

            Device (HDAS)
            {
                Name (_ADR, 0x001F0003)  // _ADR: Address
                Name (_DDN, "Audio Controller")  // _DDN: DOS Device Name
                Name (UUID, ToUUID ("a69f886e-6ceb-4594-a41f-7b5dce24c553"))
                Name (_S0W, 0x03)  // _S0W: S0 Device Wake State
                Name (NBUF, ResourceTemplate ()
                {
                    QWordMemory (ResourceConsumer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadOnly,
                        0x0000000000000000, // Granularity
                        0x0000000000000000, // Range Minimum
                        0x0000000000000000, // Range Maximum
                        0x0000000000000000, // Translation Offset
                        0x0000000000000001, // Length
                        ,, _Y0A, AddressRangeACPI, TypeStatic)
                })
                Method (_DSM, 4, NotSerialized)  // _DSM: Device-Specific Method
                {
                    If ((Arg0 == UUID))
                    {
                        If ((Arg2 == Zero))
                        {
                            If (((Arg1 == One) && ((NHLA != Zero) && (
                                NHLL != Zero))))
                            {
                                Return (Buffer (One)
                                {
                                     0x03                                             // .
                                })
                            }
                            Else
                            {
                                Return (Buffer (One)
                                {
                                     0x01                                             // .
                                })
                            }
                        }

                        If ((Arg2 == One))
                        {
                            CreateQWordField (NBUF, \_SB.PCI0.HDAS._Y0A._MIN, NBAS)  // _MIN: Minimum Base Address
                            CreateQWordField (NBUF, \_SB.PCI0.HDAS._Y0A._MAX, NMAS)  // _MAX: Maximum Base Address
                            CreateQWordField (NBUF, \_SB.PCI0.HDAS._Y0A._LEN, NLEN)  // _LEN: Length
                            NBAS = NHLA /* \NHLA */
                            NMAS = NHLA /* \NHLA */
                            NLEN = NHLL /* \NHLL */
                            Return (NBUF) /* \_SB_.PCI0.HDAS.NBUF */
                        }
                    }

                    Return (Buffer (One)
                    {
                         0x00                                             // .
                    })
                }
            }

            Method (IRQM, 1, Serialized)
            {
                Name (IQAA, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x10
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x11
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x12
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x13
                    }
                })
                Name (IQAP, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x0A
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x0B
                    }
                })
                Name (IQBA, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x11
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x12
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x13
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x10
                    }
                })
                Name (IQBP, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x0A
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x0B
                    }
                })
                Name (IQCA, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x12
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x13
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x10
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x11
                    }
                })
                Name (IQCP, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x0A
                    }
                })
                Name (IQDA, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x13
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x10
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x11
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x12
                    }
                })
                Name (IQDP, Package (0x04)
                {
                    Package (0x04)
                    {
                        0xFFFF, 
                        Zero, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        One, 
                        Zero, 
                        0x0B
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x02, 
                        Zero, 
                        0x0A
                    }, 

                    Package (0x04)
                    {
                        0xFFFF, 
                        0x03, 
                        Zero, 
                        0x0B
                    }
                })
                Switch (ToInteger (Arg0))
                {
                    Case (Package (0x04)
                        {
                            One, 
                            0x05, 
                            0x09, 
                            0x0D
                        }

)
                    {
                        If (PICM)
                        {
                            Return (IQAA) /* \_SB_.PCI0.IRQM.IQAA */
                        }
                        Else
                        {
                            Return (IQAP) /* \_SB_.PCI0.IRQM.IQAP */
                        }
                    }
                    Case (Package (0x04)
                        {
                            0x02, 
                            0x06, 
                            0x0A, 
                            0x0E
                        }

)
                    {
                        If (PICM)
                        {
                            Return (IQBA) /* \_SB_.PCI0.IRQM.IQBA */
                        }
                        Else
                        {
                            Return (IQBP) /* \_SB_.PCI0.IRQM.IQBP */
                        }
                    }
                    Case (Package (0x04)
                        {
                            0x03, 
                            0x07, 
                            0x0B, 
                            0x0F
                        }

)
                    {
                        If (PICM)
                        {
                            Return (IQCA) /* \_SB_.PCI0.IRQM.IQCA */
                        }
                        Else
                        {
                            Return (IQCP) /* \_SB_.PCI0.IRQM.IQCP */
                        }
                    }
                    Case (Package (0x04)
                        {
                            0x04, 
                            0x08, 
                            0x0C, 
                            0x10
                        }

)
                    {
                        If (PICM)
                        {
                            Return (IQDA) /* \_SB_.PCI0.IRQM.IQDA */
                        }
                        Else
                        {
                            Return (IQDP) /* \_SB_.PCI0.IRQM.IQDP */
                        }
                    }
                    Default
                    {
                        If (PICM)
                        {
                            Return (IQDA) /* \_SB_.PCI0.IRQM.IQDA */
                        }
                        Else
                        {
                            Return (IQDP) /* \_SB_.PCI0.IRQM.IQDP */
                        }
                    }

                }
            }

            Device (RP01)
            {
                Name (_ADR, 0x001C0000)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP02)
            {
                Name (_ADR, 0x001C0001)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP03)
            {
                Name (_ADR, 0x001C0002)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP04)
            {
                Name (_ADR, 0x001C0003)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP05)
            {
                Name (_ADR, 0x001C0004)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP06)
            {
                Name (_ADR, 0x001C0005)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP07)
            {
                Name (_ADR, 0x001C0006)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP08)
            {
                Name (_ADR, 0x001C0007)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP09)
            {
                Name (_ADR, 0x001D0000)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP10)
            {
                Name (_ADR, 0x001D0001)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP11)
            {
                Name (_ADR, 0x001D0002)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP12)
            {
                Name (_ADR, 0x001D0003)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP13)
            {
                Name (_ADR, 0x001D0004)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP14)
            {
                Name (_ADR, 0x001D0005)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP15)
            {
                Name (_ADR, 0x001D0006)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (RP16)
            {
                Name (_ADR, 0x001D0007)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (TBT1)
            {
                Name (_ADR, 0x001D0008)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (TBT2)
            {
                Name (_ADR, 0x001D0009)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (TBT3)
            {
                Name (_ADR, 0x001D000A)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (TBT4)
            {
                Name (_ADR, 0x001D000B)  // _ADR: Address
                OperationRegion (RPCS, PCI_Config, 0x4C, 0x04)
                Field (RPCS, AnyAcc, NoLock, Preserve)
                {
                    Offset (0x03), 
                    RPPN,   8
                }

                Method (_PRT, 0, NotSerialized)  // _PRT: PCI Routing Table
                {
                    Return (IRQM (RPPN))
                }
            }

            Device (IPC1)
            {
                Name (_HID, "INT34D2")  // _HID: Hardware ID
                Name (_CID, "INT34D2")  // _CID: Compatible ID
                Name (_DDN, "Intel(R) IPC1 Controller")  // _DDN: DOS Device Name
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite,
                        0xFE000000,         // Address Base
                        0x00010000,         // Address Length
                        )
                    IO (Decode16,
                        0x1800,             // Range Minimum
                        0x1880,             // Range Maximum
                        0x04,               // Alignment
                        0x80,               // Length
                        )
                    Interrupt (ResourceConsumer, Level, ActiveLow, Exclusive, ,, )
                    {
                        0x00000012,
                    }
                })
                Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
                {
                    Return (RBUF) /* \_SB_.PCI0.IPC1.RBUF */
                }
            }

            Device (I2C0)
            {
                Name (_ADR, 0x00150000)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 0")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C1)
            {
                Name (_ADR, 0x00150001)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 1")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C2)
            {
                Name (_ADR, 0x00150002)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 2")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C3)
            {
                Name (_ADR, 0x00150003)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 3")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C4)
            {
                Name (_ADR, 0x00190000)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 4")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C5)
            {
                Name (_ADR, 0x00190001)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 5")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C6)
            {
                Name (_ADR, 0x00100000)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 6")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (I2C7)
            {
                Name (_ADR, 0x00100001)  // _ADR: Address
                Name (_DDN, "Serial IO I2C Controller 7")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI0)
            {
                Name (_ADR, 0x001E0002)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 0")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI1)
            {
                Name (_ADR, 0x001E0003)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 1")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI2)
            {
                Name (_ADR, 0x00120006)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 2")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI3)
            {
                Name (_ADR, 0x00130000)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 3")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI4)
            {
                Name (_ADR, 0x00130001)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 4")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI5)
            {
                Name (_ADR, 0x00130002)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 5")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SPI6)
            {
                Name (_ADR, 0x00130003)  // _ADR: Address
                Name (_DDN, "Serial IO SPI Controller 6")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR0)
            {
                Name (_ADR, 0x001E0000)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 0")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR1)
            {
                Name (_ADR, 0x001E0001)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 1")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR2)
            {
                Name (_ADR, 0x00190002)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 2")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR3)
            {
                Name (_ADR, 0x00110000)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 3")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR4)
            {
                Name (_ADR, 0x00110001)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 4")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR5)
            {
                Name (_ADR, 0x00110002)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 5")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (UAR6)
            {
                Name (_ADR, 0x00110003)  // _ADR: Address
                Name (_DDN, "Serial IO UART Controller 6")  // _DDN: DOS Device Name
                Method (_PS0, 0, NotSerialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, NotSerialized)  // _PS3: Power State 3
                {
                }
            }

            Device (SBUS)
            {
                Name (_ADR, 0x001F0004)  // _ADR: Address
            }

            Device (XHCI)
            {
                Name (_ADR, 0x00140000)  // _ADR: Address
                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x6D, 
                    0x03
                })
                Name (_S3D, 0x03)  // _S3D: S3 Device State
                Name (_S0W, 0x03)  // _S0W: S0 Device Wake State
                Name (_S3W, 0x03)  // _S3W: S3 Device Wake State
                Method (_PS0, 0, Serialized)  // _PS0: Power State 0
                {
                }

                Method (_PS3, 0, Serialized)  // _PS3: Power State 3
                {
                }

                Device (RHUB)
                {
                    Name (_ADR, Zero)  // _ADR: Address
                    Device (HS01)
                    {
                        Name (_ADR, One)  // _ADR: Address
                    }

                    Device (HS02)
                    {
                        Name (_ADR, 0x02)  // _ADR: Address
                    }

                    Device (HS03)
                    {
                        Name (_ADR, 0x03)  // _ADR: Address
                    }

                    Device (HS04)
                    {
                        Name (_ADR, 0x04)  // _ADR: Address
                    }

                    Device (HS05)
                    {
                        Name (_ADR, 0x05)  // _ADR: Address
                    }

                    Device (HS06)
                    {
                        Name (_ADR, 0x06)  // _ADR: Address
                    }

                    Device (HS07)
                    {
                        Name (_ADR, 0x07)  // _ADR: Address
                    }

                    Device (HS08)
                    {
                        Name (_ADR, 0x08)  // _ADR: Address
                    }

                    Device (HS09)
                    {
                        Name (_ADR, 0x09)  // _ADR: Address
                    }

                    Device (HS10)
                    {
                        Name (_ADR, 0x0A)  // _ADR: Address
                    }

                    Device (HS11)
                    {
                        Name (_ADR, 0x0B)  // _ADR: Address
                    }

                    Device (HS12)
                    {
                        Name (_ADR, 0x0C)  // _ADR: Address
                    }

                    Device (USR1)
                    {
                        Name (_ADR, 0x0B)  // _ADR: Address
                    }

                    Device (USR2)
                    {
                        Name (_ADR, 0x0C)  // _ADR: Address
                    }

                    Device (SS01)
                    {
                        Name (_ADR, 0x0D)  // _ADR: Address
                    }

                    Device (SS02)
                    {
                        Name (_ADR, 0x0E)  // _ADR: Address
                    }

                    Device (SS03)
                    {
                        Name (_ADR, 0x0F)  // _ADR: Address
                    }

                    Device (SS04)
                    {
                        Name (_ADR, 0x10)  // _ADR: Address
                    }

                    Device (SS05)
                    {
                        Name (_ADR, 0x11)  // _ADR: Address
                    }

                    Device (SS06)
                    {
                        Name (_ADR, 0x12)  // _ADR: Address
                    }
                }
            }

            Device (CNVW)
            {
                Name (_ADR, 0x00140003)  // _ADR: Address
                OperationRegion (CWAR, PCI_Config, Zero, 0x0100)
                Field (CWAR, WordAcc, NoLock, Preserve)
                {
                    VDID,   32, 
                        ,   1, 
                    WMSE,   1, 
                    WBME,   1, 
                    Offset (0x10), 
                    WBR0,   64, 
                    Offset (0x44), 
                        ,   28, 
                    WFLR,   1, 
                    Offset (0x48), 
                        ,   15, 
                    WIFR,   1, 
                    Offset (0xCC), 
                    WPMS,   32
                }

                Method (_S0W, 0, NotSerialized)  // _S0W: S0 Device Wake State
                {
                    Return (0x03)
                }

                Method (_DSW, 3, NotSerialized)  // _DSW: Device Sleep Wake
                {
                }

                PowerResource (WRST, 0x05, 0x0000)
                {
                    Method (_STA, 0, NotSerialized)  // _STA: Status
                    {
                        Return (One)
                    }

                    Method (_ON, 0, NotSerialized)  // _ON_: Power On
                    {
                    }

                    Method (_OFF, 0, NotSerialized)  // _OFF: Power Off
                    {
                    }

                    Method (_RST, 0, NotSerialized)  // _RST: Device Reset
                    {
                        If ((WFLR == One))
                        {
                            WBR0 = Zero
                            WPMS = Zero
                            WBME = Zero
                            WMSE = Zero
                            WIFR = One
                        }
                    }
                }

                Name (_PRR, Package (0x01)  // _PRR: Power Resource for Reset
                {
                    WRST
                })
            }

            Method (CNIP, 0, NotSerialized)
            {
                If ((^CNVW.VDID != 0xFFFFFFFF))
                {
                    Return (One)
                }
                Else
                {
                    Return (Zero)
                }
            }

            Method (SBTE, 1, Serialized)
            {
                Local0 = 0x90
                If ((One & Arg0))
                {
                    STXS (Local0)
                }
                Else
                {
                    CTXS (Local0)
                }
            }

            Method (GBTE, 0, NotSerialized)
            {
                Local0 = 0x90
                GTXS (Local0)
            }

            Method (AOLX, 0, NotSerialized)
            {
                Name (AODS, Package (0x03)
                {
                    Zero, 
                    0x12, 
                    Zero
                })
                Return (AODS) /* \_SB_.PCI0.AOLX.AODS */
            }

            If (CNIP ())
            {
                Scope (XHCI.RHUB.HS10)
                {
                    Method (AOLD, 0, NotSerialized)
                    {
                        Return (AOLX ())
                    }
                }
            }

            Scope (\_SB.PCI0)
            {
                Method (_OSC, 4, NotSerialized)  // _OSC: Operating System Capabilities
                {
                    If ((Arg0 == ToUUID ("33db4d5b-1ff7-401c-9657-7441c03dd766") /* PCI Host Bridge Device */))
                    {
                        Return (Arg3)
                    }
                    Else
                    {
                        CreateDWordField (Arg3, Zero, CDW1)
                        CDW1 |= 0x04
                        Return (Arg3)
                    }
                }
            }

            Device (GLAN)
            {
                Name (_ADR, 0x001F0006)  // _ADR: Address
                Name (_S0W, 0x03)  // _S0W: S0 Device Wake State
                Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
                {
                    0x6D, 
                    0x04
                })
                Method (_DSW, 3, NotSerialized)  // _DSW: Device Sleep Wake
                {
                }
            }
        }
    }

    Scope (_SB.PCI0)
    {
        Device (IPU0)
        {
            Name (_ADR, 0x00050000)  // _ADR: Address
            Name (_DDN, "Camera and Imaging Subsystem")  // _DDN: DOS Device Name
        }
    }

    Scope (_SB.PCI0.IPU0)
    {
        Name (_DSD, Package (0x02)  // _DSD: Device-Specific Data
        {
            ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
            Package (0x02)
            {
                Package (0x02)
                {
                    "port0", 
                    "PRT0"
                }, 

                Package (0x02)
                {
                    "port1", 
                    "PRT1"
                }
            }
        })
        Name (PRT0, Package (0x04)
        {
            ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
            Package (0x01)
            {
                Package (0x02)
                {
                    "port", 
                    One
                }
            }, 

            ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
            Package (0x01)
            {
                Package (0x02)
                {
                    "endpoint0", 
                    "EP00"
                }
            }
        })
        Name (PRT1, Package (0x04)
        {
            ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
            Package (0x01)
            {
                Package (0x02)
                {
                    "port", 
                    0x02
                }
            }, 

            ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
            Package (0x01)
            {
                Package (0x02)
                {
                    "endpoint0", 
                    "EP10"
                }
            }
        })
    }

    Scope (_SB.PCI0.IPU0)
    {
        Name (EP00, Package (0x02)
        {
            ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
            Package (0x04)
            {
                Package (0x02)
                {
                    "endpoint", 
                    Zero
                }, 

                Package (0x02)
                {
                    "clock-lanes", 
                    Zero
                }, 

                Package (0x02)
                {
                    "data-lanes", 
                    Package (0x01)
                    {
                        One
                    }
                }, 

                Package (0x02)
                {
                    "remote-endpoint", 
                    Package (0x03)
                    {
                        ^I2C3.CAM0, 
                        Zero, 
                        Zero
                    }
                }
            }
        })
        Name (EP10, Package (0x02)
        {
            ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
            Package (0x04)
            {
                Package (0x02)
                {
                    "endpoint", 
                    Zero
                }, 

                Package (0x02)
                {
                    "clock-lanes", 
                    Zero
                }, 

                Package (0x02)
                {
                    "data-lanes", 
                    Package (0x01)
                    {
                        One
                    }
                }, 

                Package (0x02)
                {
                    "remote-endpoint", 
                    Package (0x03)
                    {
                        ^I2C5.CAM1, 
                        Zero, 
                        Zero
                    }
                }
            }
        })
    }

    Scope (_SB.PCI0.I2C3)
    {
        PowerResource (RCPR, 0x00, 0x0000)
        {
            Name (STA, Zero)
            Method (_ON, 0, Serialized)  // _ON_: Power On
            {
                If ((STA == Zero))
                {
                    MCCT (Zero, One, One)
                    STXS (0x17)
                    Sleep (0x05)
                    CTXS (0xCB)
                    Sleep (0x05)
                    STXS (0xCB)
                    Sleep (0x05)
                    STA = One
                }
            }

            Method (_OFF, 0, Serialized)  // _OFF: Power Off
            {
                If ((STA == One))
                {
                    CTXS (0xCB)
                    CTXS (0x17)
                    MCCT (Zero, Zero, One)
                    STA = Zero
                }
            }

            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STA) /* \_SB_.PCI0.I2C3.RCPR.STA_ */
            }
        }

        Device (CAM0)
        {
            Name (_HID, "OVTI01AF")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DDN, "OV 01A1S Camera")  // _DDN: DOS Device Name
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                I2cSerialBusV2 (0x0036, ControllerInitiated, 0x00061A80,
                    AddressingMode7Bit, "\\_SB.PCI0.I2C3",
                    0x00, ResourceConsumer, , Exclusive,
                    )
            })
            Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
            {
                RCPR
            })
            Name (_PR3, Package (0x01)  // _PR3: Power Resources for D3hot
            {
                RCPR
            })
            Name (_DSD, Package (0x04)  // _DSD: Device-Specific Data
            {
                ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "port0", 
                        "PRT0"
                    }
                }, 

                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x0124F800
                    }
                }
            })
            Name (PRT0, Package (0x04)
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "port", 
                        Zero
                    }
                }, 

                ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "endpoint0", 
                        "EP00"
                    }
                }
            })
            Name (EP00, Package (0x02)
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x03)
                {
                    Package (0x02)
                    {
                        "endpoint", 
                        Zero
                    }, 

                    Package (0x02)
                    {
                        "link-frequencies", 
                        Package (0x01)
                        {
                            0x325AA000
                        }
                    }, 

                    Package (0x02)
                    {
                        "remote-endpoint", 
                        Package (0x03)
                        {
                            IPU0, 
                            Zero, 
                            Zero
                        }
                    }
                }
            })
        }
    }

    Scope (_SB.PCI0.I2C5)
    {
        PowerResource (FCPR, 0x00, 0x0000)
        {
            Name (STA, Zero)
            Method (_ON, 0, Serialized)  // _ON_: Power On
            {
                If ((STA == Zero))
                {
                    MCCT (One, One, One)
                    STXS (0x011C)
                    Sleep (0x05)
                    CTXS (0x57)
                    Sleep (0x05)
                    STXS (0x57)
                    Sleep (0x05)
                    STA = One
                }
            }

            Method (_OFF, 0, Serialized)  // _OFF: Power Off
            {
                If ((STA == One))
                {
                    CTXS (0x57)
                    CTXS (0x011C)
                    MCCT (One, Zero, One)
                    STA = Zero
                }
            }

            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (STA) /* \_SB_.PCI0.I2C5.FCPR.STA_ */
            }
        }

        Device (CAM1)
        {
            Name (_HID, "OVTI01AF")  // _HID: Hardware ID
            Name (_UID, Zero)  // _UID: Unique ID
            Name (_DDN, "OV 01A1S Camera")  // _DDN: DOS Device Name
            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0F)
            }

            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                I2cSerialBusV2 (0x0036, ControllerInitiated, 0x00061A80,
                    AddressingMode7Bit, "\\_SB.PCI0.I2C5",
                    0x00, ResourceConsumer, , Exclusive,
                    )
            })
            Name (_PR0, Package (0x01)  // _PR0: Power Resources for D0
            {
                FCPR
            })
            Name (_PR3, Package (0x01)  // _PR3: Power Resources for D3hot
            {
                FCPR
            })
            Name (_DSD, Package (0x04)  // _DSD: Device-Specific Data
            {
                ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "port0", 
                        "PRT0"
                    }
                }, 

                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "clock-frequency", 
                        0x0124F800
                    }
                }
            })
            Name (PRT0, Package (0x04)
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "port", 
                        Zero
                    }
                }, 

                ToUUID ("dbb8e3e6-5886-4ba6-8795-1319f52a966b"), 
                Package (0x01)
                {
                    Package (0x02)
                    {
                        "endpoint0", 
                        "EP00"
                    }
                }
            })
            Name (EP00, Package (0x02)
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301") /* Device Properties for _DSD */, 
                Package (0x03)
                {
                    Package (0x02)
                    {
                        "endpoint", 
                        Zero
                    }, 

                    Package (0x02)
                    {
                        "link-frequencies", 
                        Package (0x01)
                        {
                            0x325AA000
                        }
                    }, 

                    Package (0x02)
                    {
                        "remote-endpoint", 
                        Package (0x03)
                        {
                            IPU0, 
                            One, 
                            Zero
                        }
                    }
                }
            })
        }
    }

    Device (CRHW)
    {
        Name (_HID, EisaId ("GGL0001"))  // _HID: Hardware ID
        Method (_STA, 0, Serialized)  // _STA: Status
        {
            Return (0x0B)
        }

        Method (CHSW, 0, Serialized)
        {
            Name (WSHC, Package (0x01)
            {
                VBT3
            })
            Return (WSHC) /* \CRHW.CHSW.WSHC */
        }

        Method (FWID, 0, Serialized)
        {
            Name (DIW1, "")
            ToString (VBT5, 0x3F, DIW1) /* \CRHW.FWID.DIW1 */
            Name (DIWF, Package (0x01)
            {
                DIW1
            })
            Return (DIWF) /* \CRHW.FWID.DIWF */
        }

        Method (FRID, 0, Serialized)
        {
            Name (DIR1, "")
            ToString (VBT6, 0x3F, DIR1) /* \CRHW.FRID.DIR1 */
            Name (DIRF, Package (0x01)
            {
                DIR1
            })
            Return (DIRF) /* \CRHW.FRID.DIRF */
        }

        Method (HWID, 0, Serialized)
        {
            Name (DIW0, "")
            ToString (VBT4, 0xFF, DIW0) /* \CRHW.HWID.DIW0 */
            Name (DIWH, Package (0x01)
            {
                DIW0
            })
            Return (DIWH) /* \CRHW.HWID.DIWH */
        }

        Method (BINF, 0, Serialized)
        {
            Name (FNIB, Package (0x05)
            {
                VBT0, 
                VBT1, 
                VBT2, 
                VBT7, 
                VBT8
            })
            Return (FNIB) /* \CRHW.BINF.FNIB */
        }

        Method (GPIO, 0, Serialized)
        {
            Return (OIPG) /* External reference */
        }

        Method (VBNV, 0, Serialized)
        {
            Name (VNBV, Package (0x02)
            {
                0x26, 
                0x10
            })
            Return (VNBV) /* \CRHW.VBNV.VNBV */
        }

        Method (VDAT, 0, Serialized)
        {
            Name (TAD0, "")
            ToBuffer (CHVD, TAD0) /* \CRHW.VDAT.TAD0 */
            Name (TADV, Package (0x01)
            {
                TAD0
            })
            Return (TADV) /* \CRHW.VDAT.TADV */
        }

        Method (FMAP, 0, Serialized)
        {
            Name (PAMF, Package (0x01)
            {
                VBT9
            })
            Return (PAMF) /* \CRHW.FMAP.PAMF */
        }

        Method (MECK, 0, Serialized)
        {
            Name (HASH, Package (0x01)
            {
                MEHH
            })
            Return (HASH) /* \CRHW.MECK.HASH */
        }

        Method (MLST, 0, Serialized)
        {
            Name (TSLM, Package (0x0A)
            {
                "CHSW", 
                "FWID", 
                "HWID", 
                "FRID", 
                "BINF", 
                "GPIO", 
                "VBNV", 
                "VDAT", 
                "FMAP", 
                "MECK"
            })
            Return (TSLM) /* \CRHW.MLST.TSLM */
        }
    }

    Scope (_SB)
    {
        Device (RMOP)
        {
            Name (_HID, "GOOG9999")  // _HID: Hardware ID
            Name (_CID, "GOOG9999")  // _CID: Compatible ID
            Name (_UID, One)  // _UID: Unique ID
            Name (RBUF, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite,
                    0x00000000,         // Address Base
                    0x00000000,         // Address Length
                    _Y0B)
            })
            Method (_CRS, 0, NotSerialized)  // _CRS: Current Resource Settings
            {
                CreateDWordField (RBUF, \_SB.RMOP._Y0B._BAS, RBAS)  // _BAS: Base Address
                CreateDWordField (RBUF, \_SB.RMOP._Y0B._LEN, RLEN)  // _LEN: Length
                RBAS = RMOB /* \RMOB */
                RLEN = RMOL /* \RMOL */
                Return (RBUF) /* \_SB_.RMOP.RBUF */
            }

            Method (_STA, 0, NotSerialized)  // _STA: Status
            {
                Return (0x0B)
            }
        }
    }

    Name (_S0, Package (0x04)  // _S0_: S0 System State
    {
        Zero, 
        Zero, 
        Zero, 
        Zero
    })
    Name (_S3, Package (0x04)  // _S3_: S3 System State
    {
        0x05, 
        0x05, 
        Zero, 
        Zero
    })
    Name (_S5, Package (0x04)  // _S5_: S5 System State
    {
        0x07, 
        0x07, 
        Zero, 
        Zero
    })
}

