// Minimal crnlib.h - provides types required by crn_decomp.h
// Extracted from the original crunch library (public domain).
#pragma once

namespace crnd
{
   const unsigned int cCRNMaxLevels = 16;
   const unsigned int cCRNMaxLevelResolution = 4096;

   enum crn_format
   {
      cCRNFmtDXT1 = 0,
      cCRNFmtDXT3,
      cCRNFmtDXT5,

      // Various swizzled DXT5 formats.
      cCRNFmtDXT5_CCxY,
      cCRNFmtDXT5_xGxR,
      cCRNFmtDXT5_xGBR,
      cCRNFmtDXT5_AGBR,

      // ATI 3DC and target DXN
      cCRNFmtDXN_XY,
      cCRNFmtDXN_YX,

      // DXT5 alpha only
      cCRNFmtDXT5A,

      cCRNFmtETC1,

      cCRNFmtTotal,
      cCRNFmtForceDWORD = 0xFFFFFFFF
   };
} // namespace crnd
