# Phoenix/Cure content format

## Common header

All integers are little-endian. The first 44 bytes have fixed offsets:

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 19 | ASCII `Phoenix -Cure`, padded with spaces |
| 19 | 18 | `-Samples`, `-Wavetables` or `-Patches`, padded with spaces |
| 37 | 3 | `0x1a 0x00 'L'` |
| 40 | 4 | Format version, u32 (1) |

The branding field can be normalised without changing any payload offsets. The loader also recognises compatible headers structurally; imports and user-bank saves write Phoenix/Cure branding.

- `.smp`: u32 count at offset 44, records at 48. Each sample has u32 rate, u32 loopStart (0 = one-shot), u32 byteLength, followed by int16 PCM.
- `.wvt`: u32 count at offset 48, records at 52. Each wavetable has char[24] name, u8 loop flag, u16 zone count, then zones of {u16 sampleBank, u16 sampleIndex, u16 rootKey, u16 highKey}. Wave ID = filePrefix × 128 + index + 1.
- `.pth`: u32 count at offset 44, records at 48. Each patch has char[24] name, u16 version (2), u16 layer count, u32 body size. Each layer has char[24] name, u32 parameter count and float32 normalised parameters. The patch ends with u32 global-parameter count (9) and nine floats; index 3 is volume, index 8 is tempo.

## Compatibility importer

The supported setup.exe contains 44 LZMA content chunks, listed in `Source/ContentManifest.inc`. The importer recognises the header structure and normalises its branding before writing the files. `Resources/Cure/SHA256SUMS` records the canonical library hashes.

DSP is independently implemented; exact sonic equivalence is not assumed.
