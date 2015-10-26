Source for these ROMs is 5620rom.cpio.Z from brouhaha.com -- they
have internal version 8;7;5 (2.0) but likely were compiled from that
source tree, not dumped, and so do not exactly match symbol tables in
"src/xt/layersys/rdpatch/lsys.nm.2.0".  MAME driver can load them thus:

ROM_LOAD32_BYTE( "combined.000.bin",  0x000003, 0x004000, CRC(87125448) SHA1(b4180740af1fa7dacb06a1d7112a6d48a9b7534c) )
ROM_LOAD32_BYTE( "combined.100.bin",  0x000002, 0x004000, CRC(93d4724f) SHA1(0aa5b5cbb3c39f4d1fabcfca2f500b9ebb8fab22) )
ROM_LOAD32_BYTE( "combined.200.bin",  0x000001, 0x004000, CRC(521bf517) SHA1(2171a73ce92c09811a99a940870ff49811ab095b) )
ROM_LOAD32_BYTE( "combined.300.bin",  0x000000, 0x004000, CRC(579a7853) SHA1(b29768a00da066e2f9f4297d877293ad6aa1f70a) )
ROM_LOAD32_BYTE( "combined.001.bin",  0x010003, 0x004000, CRC(0fc3120c) SHA1(e149eec9374ba45c579cc270604c9c0bc9607f59) )
ROM_LOAD32_BYTE( "combined.101.bin",  0x010002, 0x004000, CRC(ec9f6d58) SHA1(b63641a6a2a355c0a2d722657932070c0b3d78a7) )
ROM_LOAD32_BYTE( "combined.201.bin",  0x010001, 0x004000, CRC(b0898601) SHA1(cc51c9cec8d242b54d178663848fca3cfc023ca7) )
ROM_LOAD32_BYTE( "combined.301.bin",  0x010000, 0x004000, CRC(8cc1e71b) SHA1(9b817df0a55dcf5442aa901601de6b5cb731c1ea) )

