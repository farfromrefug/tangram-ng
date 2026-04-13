#include "util/zlibHelper.h"

#ifndef TANGRAM_NO_WUFFS
#include "wuffs.h"
#include "log.h"
#else
#include "miniz.h"
#endif

namespace Tangram {

int zlib_inflate(const char* _data, size_t _size, std::vector<char>& dst) {

    // miniz does not handle gzip - we will check for header and assume no extra header data; we can use
    //  miniz_gzip.h for more robust handling in the future
    if (_size < 2) { return -1; }
    uint8_t d1 = (uint8_t)_data[1];
    if (_size > 10 && _data[0] == 0x1F && d1 == 0x8B) {
        // last 4 bytes of gzip file contain uncompressed size
        union { uint8_t bytes[4]; uint32_t dword; } inflsize;
        memcpy(inflsize.bytes, &_data[_size-4], 4);
        dst.resize(std::min(uint32_t(10*_size), inflsize.dword));
        //if(_data[3] & 0b0001'1110)  LOGE("Extra header fields present");
        _data += 10;
        _size -= 10;  //18;  -- in case footer is missing
    }
    // check for raw zlib header
    else if (_size > 7 && _data[0] == 0x78 && (d1 == 0x01 || d1 == 0x5E || d1 == 0x9C || d1 == 0xDA)) {
        _data += 2;  // need to advance for WUFFS (but not zlib)
        _size -= 2;
        dst.resize(_size*4);
    }
//#ifndef TANGRAM_TILE_RAW_DEFLATE
    else { return -1; }
//#endif

#ifndef TANGRAM_NO_WUFFS
    auto dec = wuffs_deflate__decoder::alloc();
    auto status = wuffs_deflate__decoder__initialize(dec.get(), sizeof__wuffs_deflate__decoder(), WUFFS_VERSION, 0);
    if (!wuffs_base__status__is_ok(&status)) { return -1; }

    auto rdbuf = wuffs_base__ptr_u8__reader((uint8_t*)_data, _size, true);
    auto wrbuf = wuffs_base__ptr_u8__writer((uint8_t*)dst.data(), dst.size());
    constexpr size_t wbsize = WUFFS_DEFLATE__DECODER_WORKBUF_LEN_MAX_INCL_WORST_CASE;  //+1 in case it's 0?
    uint8_t workbuf[wbsize];
    auto wbslice = wuffs_base__make_slice_u8(workbuf, wbsize);  //(nullptr, 0)

    while (true) {
        status = wuffs_deflate__decoder__transform_io(dec.get(), &wrbuf, &rdbuf, wbslice);
        if (wuffs_base__status__is_ok(&status)) {
            dst.resize(wrbuf.meta.wi);
            return 0;
        } else if (status.repr != wuffs_base__suspension__short_write) {
            LOGE("WUFFS error: %s", wuffs_base__status__message(&status));
            return -1;
        }
        dst.resize(2*dst.size());
        wrbuf.data.ptr = (uint8_t*)dst.data();
        wrbuf.data.len = dst.size();
    }
#else
    z_stream strm;
    memset(&strm, 0, sizeof(z_stream));

    int ret = inflateInit2(&strm, -MZ_DEFAULT_WINDOW_BITS);
    if (ret != Z_OK) { return ret; }

    strm.avail_in = _size;
    strm.next_in = (Bytef*)_data;

    while (true) {
        strm.avail_out = dst.size() - strm.total_out;
        strm.next_out = (uint8_t*)dst.data() + strm.total_out;

        ret = inflate(&strm, Z_NO_FLUSH);

        if(ret != Z_OK) {
            dst.resize(strm.total_out);
            break;
        }
        dst.resize(2*dst.size());
    }

    inflateEnd(&strm);

    return ret == Z_STREAM_END ? Z_OK : ret;
#endif
}

}
