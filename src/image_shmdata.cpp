#include "image_shmdata.h"
#include "timer.h"
#include "threadpool.h"

#include <regex>

#define SPLASH_SHMDATA_THREADS 16

using namespace std;

namespace Splash
{

/*************/
Image_Shmdata::Image_Shmdata()
{
    _type = "image_shmdata";

    registerAttributes();
}

/*************/
Image_Shmdata::~Image_Shmdata()
{
    if (_reader != nullptr)
        shmdata_any_reader_close(_reader);
    if (_writer != nullptr)
        shmdata_any_writer_close(_writer);

#ifdef DEBUG
    SLog::log << Log::DEBUGGING << "Image_Shmdata::~Image_Shmdata - Destructor" << Log::endl;
#endif
}

/*************/
bool Image_Shmdata::read(const string& filename)
{
    if (_reader != nullptr)
        shmdata_any_reader_close(_reader);

    _reader = shmdata_any_reader_init();
    shmdata_any_reader_run_gmainloop(_reader, SHMDATA_TRUE);
    shmdata_any_reader_set_on_data_handler(_reader, Image_Shmdata::onData, this);
    shmdata_any_reader_start(_reader, filename.c_str());
    _filename = filename;

    return true;
}

/*************/
bool Image_Shmdata::write(const oiio::ImageBuf& img, const string& filename)
{
    if (img.localpixels() == NULL)
        return false;

    lock_guard<mutex> lock(_mutex);
    oiio::ImageSpec spec = img.spec();
    if (spec.width != _writerSpec.width || spec.height != _writerSpec.height || spec.nchannels != _writerSpec.nchannels || _writer == NULL || _filename != filename)
        if (!initShmWriter(spec, filename))
            return false;

    memcpy(_writerBuffer.localpixels(), img.localpixels(), _writerInputSize);
    unsigned long long currentTime = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();
    shmdata_any_writer_push_data(_writer, (void*)_writerBuffer.localpixels(), _writerInputSize, (currentTime - _writerStartTime) * 1e6, NULL, NULL);
    return true;
}

/*************/
bool Image_Shmdata::initShmWriter(const oiio::ImageSpec& spec, const string& filename)
{
    if (_writer != NULL)
        shmdata_any_writer_close(_writer);

    _writer = shmdata_any_writer_init();
    
    string dataType;
    if (spec.format == "uint8" && spec.nchannels == 4)
    {
        dataType += "video/x-raw-rgb,bpp=32,endianness=4321,depth=32,red_mask=-16777216,green_mask=16711680,blue_mask=65280,";
        _writerInputSize = 4;
    }
    else if (spec.format == "uint16" && spec.nchannels == 1)
    {
        dataType += "video/x-raw-gray,bpp=16,endianness=4321,depth=16,";
        _writerInputSize = 2;
    }
    else
    {
        _writerInputSize = 0;
        return false;
    }

    dataType += "width=" + to_string(spec.width) + ",";
    dataType += "height=" + to_string(spec.height) + ",";
    dataType += "framerate=60/1";
    _writerInputSize *= spec.width * spec.height;

    shmdata_any_writer_set_data_type(_writer, dataType.c_str());
    if (!shmdata_any_writer_set_path(_writer, filename.c_str()))
    {
        SLog::log << Log::WARNING << "Image_Shmdata::" << __FUNCTION__ << " - Unable to write to shared memory " << filename << Log::endl;
        _filename = "";
        return false;
    }

    _filename = filename;
    _writerSpec = spec;
    shmdata_any_writer_start(_writer);
    _writerStartTime = chrono::duration_cast<chrono::milliseconds>(chrono::high_resolution_clock::now().time_since_epoch()).count();

    _writerBuffer.reset(_writerSpec);

    return true;
}

/*************/
void Image_Shmdata::onData(shmdata_any_reader_t* reader, void* shmbuf, void* data, int data_size, unsigned long long timestamp,
    const char* type_description, void* user_data)
{
    Image_Shmdata* ctx = static_cast<Image_Shmdata*>(user_data);

    STimer::timer << "image_shmdata " + ctx->_name;

    string dataType(type_description);
    if (dataType != ctx->_inputDataType)
    {
        ctx->_inputDataType = dataType;

        ctx->_bpp = 0;
        ctx->_width = 0;
        ctx->_height = 0;
        ctx->_red = 0;
        ctx->_green = 0;
        ctx->_blue = 0;
        ctx->_channels = 0;
        ctx->_isYUV = false;
        ctx->_is420 = false;
        
        regex regRgb, regGray, regYUV, regBpp, regWidth, regHeight, regRed, regBlue, regFormatYUV;
        try
        {
            // TODO: replace these regex with some GCC 4.7 compatible ones
            // GCC 4.6 does not support the full regular expression. Some work around is needed,
            // this is why this may seem complicated for nothing ...
            regRgb = regex("(video/x-raw-rgb)(.*)", regex_constants::extended);
            regYUV = regex("(video/x-raw-yuv)(.*)", regex_constants::extended);
            regFormatYUV = regex("(.*format=\\(fourcc\\))(.*)", regex_constants::extended);
            regBpp = regex("(.*bpp=\\(int\\))(.*)", regex_constants::extended);
            regWidth = regex("(.*width=\\(int\\))(.*)", regex_constants::extended);
            regHeight = regex("(.*height=\\(int\\))(.*)", regex_constants::extended);
            regRed = regex("(.*red_mask=\\(int\\))(.*)", regex_constants::extended);
            regBlue = regex("(.*blue_mask=\\(int\\))(.*)", regex_constants::extended);
        }
        catch (const regex_error& e)
        {
            SLog::log << Log::WARNING << "Image_Shmdata::" << __FUNCTION__ << " - Regex error code: " << e.code() << Log::endl;
            shmdata_any_reader_free(shmbuf);
            return;
        }

        if (regex_match(dataType, regRgb) || regex_match(dataType, regYUV))
        {

            smatch match;
            string substr, format;

            if (regex_match(dataType, match, regBpp))
            {
                ssub_match subMatch = match[2];
                substr = subMatch.str();
                sscanf(substr.c_str(), ")%i", &ctx->_bpp);
            }
            if (regex_match(dataType, match, regWidth))
            {
                ssub_match subMatch = match[2];
                substr = subMatch.str();
                sscanf(substr.c_str(), ")%i", &ctx->_width);
            }
            if (regex_match(dataType, match, regHeight))
            {
                ssub_match subMatch = match[2];
                substr = subMatch.str();
                sscanf(substr.c_str(), ")%i", &ctx->_height);
            }
            if (regex_match(dataType, match, regRed))
            {
                ssub_match subMatch = match[2];
                substr = subMatch.str();
                sscanf(substr.c_str(), ")%i", &ctx->_red);
            }
            else if (regex_match(dataType, regYUV))
            {
                ctx->_isYUV = true;
            }
            if (regex_match(dataType, match, regBlue))
            {
                ssub_match subMatch = match[2];
                substr = subMatch.str();
                sscanf(substr.c_str(), ")%i", &ctx->_blue);
            }

            if (ctx->_bpp == 24)
                ctx->_channels = 3;
            else if (ctx->_isYUV)
            {
                ctx->_bpp = 12;
                ctx->_channels = 3;

                if (regex_match(dataType, match, regFormatYUV))
                {
                    char format[16];
                    ssub_match subMatch = match[2];
                    substr = subMatch.str();
                    sscanf(substr.c_str(), ")%s", format);
                    if (strstr(format, (char*)"I420") != nullptr)
                        ctx->_is420 = true;
                }
            }
        }
    }

    if (ctx->_width != 0 && ctx->_height != 0 && ctx->_bpp != 0 && ctx->_channels != 0)
    {
        // Check if we need to resize the reader buffer
        oiio::ImageSpec bufSpec = ctx->_readerBuffer.spec();
        if (bufSpec.width != ctx->_width || bufSpec.height != ctx->_height || bufSpec.nchannels != ctx->_channels)
        {
            oiio::ImageSpec spec(ctx->_width, ctx->_height, ctx->_channels, oiio::TypeDesc::UINT8);
            ctx->_readerBuffer.reset(spec);
        }

        if (!ctx->_is420 && (ctx->_channels == 3 || ctx->_channels == 4))
        {
            char* pixels = (char*)(ctx->_readerBuffer).localpixels();
            memcpy(pixels, (const char*)data, ctx->_width * ctx->_height * ctx->_channels * sizeof(char));
        }
        else if (ctx->_is420)
        {
            unsigned char* Y = (unsigned char*)data;
            unsigned char* U = (unsigned char*)data + ctx->_width * ctx->_height;
            unsigned char* V = (unsigned char*)data + ctx->_width * ctx->_height * 5 / 4;

            char* pixels = (char*)(ctx->_readerBuffer).localpixels();
            vector<unsigned int> threadIds;
            for (int block = 0; block < SPLASH_SHMDATA_THREADS; ++block)
            {
                threadIds.push_back(SThread::pool.enqueue([=, &ctx]() {
                    for (int y = ctx->_height / SPLASH_SHMDATA_THREADS * block; y < ctx->_height / SPLASH_SHMDATA_THREADS * (block + 1); y++)
                        for (int x = 0; x < ctx->_width; x+=2)
                        {
                            int uValue = (int)(U[(y / 2) * (ctx->_width / 2) + x / 2]) - 128;
                            int vValue = (int)(V[(y / 2) * (ctx->_width / 2) + x / 2]) - 128;

                            int rPart = 52298 * vValue;
                            int gPart = -12846 * uValue - 36641 * vValue;
                            int bPart = 66094 * uValue;
                           
                            int col = x;
                            int row = y;
                            int yValue = (int)(Y[row * ctx->_width + col]) * 38142;
                            pixels[(row * ctx->_width + col) * 3] = (unsigned char)clamp((yValue + rPart) / 32768, 0, 255);
                            pixels[(row * ctx->_width + col) * 3 + 1] = (unsigned char)clamp((yValue + gPart) / 32768, 0, 255);
                            pixels[(row * ctx->_width + col) * 3 + 2] = (unsigned char)clamp((yValue + bPart) / 32768, 0, 255);

                            col++;
                            yValue = (int)(Y[row * ctx->_width + col]) * 38142;
                            pixels[(row * ctx->_width + col) * 3] = (unsigned char)clamp((yValue + rPart) / 32768, 0, 255);
                            pixels[(row * ctx->_width + col) * 3 + 1] = (unsigned char)clamp((yValue + gPart) / 32768, 0, 255);
                            pixels[(row * ctx->_width + col) * 3 + 2] = (unsigned char)clamp((yValue + bPart) / 32768, 0, 255);

                        }
                }));
            }
            SThread::pool.waitThreads(threadIds);
        }
        else
        {
            shmdata_any_reader_free(shmbuf);
            return;
        }

        lock_guard<mutex> lock(ctx->_mutex);
        ctx->_bufferImage.swap(ctx->_readerBuffer);
        ctx->_imageUpdated = true;
        ctx->updateTimestamp();
    }

    shmdata_any_reader_free(shmbuf);

    STimer::timer >> "image_shmdata " + ctx->_name;
}

/*************/
void Image_Shmdata::registerAttributes()
{
    _attribFunctions["file"] = AttributeFunctor([&](vector<Value> args) {
        if (args.size() < 1)
            return false;
        return read(args[0].asString());
    });
}

}
