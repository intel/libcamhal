/*
 * Copyright (C) 2016-2018 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "EXIFMaker"

#include <limits.h>

#include "EXIFMaker.h"

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ParameterHelper.h"

namespace icamera {

#define DEFAULT_ISO_SPEED 100

EXIFMaker::EXIFMaker() :
    mExifSize(-1)
    ,mInitialized(false)
{
    LOG1("@%s", __FUNCTION__);
    CLEAR(mExifAttributes);
    mMakernoteSection = new unsigned char[MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE];
}

EXIFMaker::~EXIFMaker()
{
    LOG1("@%s", __FUNCTION__);
    delete [] mMakernoteSection;
}


/**
 * Fills EXIF data after a picture has been taken to
 * record the active sensor, 3A and ISP state to EXIF metadata.
 *
 * This function is intented to set EXIF tags belonging
 * to the EXIF "Per Picture Camera Setting" group.
 *
 * @arg params active Android HAL parameters
 */
void EXIFMaker::pictureTaken(ExifMetaData *exifmetadata)
{
    LOG1("@%s", __FUNCTION__);

    // brightness, -99.99 to 99.99. FFFFFFFF.H means unknown.
    float brightness;
    // TODO: The check for getAeManualBrightness of 3A should be moved
    //       to MetaData class, because the metadata collection happen
    //       at capture time
    brightness = 99;
    mExifAttributes.brightness.num = static_cast<int>(brightness * 100);
    mExifAttributes.brightness.den = 100;
    LOG1("EXIF: brightness = %.2f", brightness);

    mExifAttributes.contrast   = 0;
    mExifAttributes.saturation = 0;
    mExifAttributes.sharpness  = 0;
    LOG1("EXIF: contrast=%d, saturation=%d, sharpness=%d (0:normal 1:low 2:high)",
            mExifAttributes.contrast,
            mExifAttributes.saturation,
            mExifAttributes.sharpness);


    mExifAttributes.exposure_program = EXIF_EXPOSURE_PROGRAM_NORMAL;
    LOG1("EXIF: Exposure Program = Normal");
    mExifAttributes.exposure_mode = EXIF_EXPOSURE_AUTO;
    LOG1("EXIF: Exposure Mode = Auto");

    mExifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;

    LOG1("EXIF: ISO=%d", mExifAttributes.iso_speed_rating);
    mExifAttributes.metering_mode = EXIF_METERING_AVERAGE;
    mExifAttributes.white_balance = EXIF_WB_AUTO;

    mExifAttributes.light_source = EXIF_LIGHT_SOURCE_UNKNOWN;

    mExifAttributes.light_source  = EXIF_LIGHT_SOURCE_OTHER_LIGHT_SOURCE;


    mExifAttributes.scene_capture_type = EXIF_SCENE_STANDARD;

    int rotation = exifmetadata->mJpegSetting.orientation;
    mExifAttributes.orientation = EXIF_ORIENTATION_UP;
    if (0 == rotation)
        mExifAttributes.orientation = EXIF_ORIENTATION_UP;
    else if (90 == rotation)
        mExifAttributes.orientation = EXIF_ORIENTATION_90;
    else if (180 == rotation)
        mExifAttributes.orientation = EXIF_ORIENTATION_180;
    else if (270 == rotation)
        mExifAttributes.orientation = EXIF_ORIENTATION_270;

    // Platform has no HW rotation. No swap here
    //if (rotation % 180 == 90)
    //    swap(mExifAttributes.width, mExifAttributes.height);

    mExifAttributes.zoom_ratio.num = exifmetadata->zoomRatio;
    mExifAttributes.zoom_ratio.den = 100;
    // the unit of subjectDistance is meter, focus distance from 3A is mm.
    mExifAttributes.subject_distance.num = 0;
    mExifAttributes.subject_distance.den = 1000;
    mExifAttributes.custom_rendered = exifmetadata->hdr ?
        EXIF_CUSTOM_RENDERED_HDR : EXIF_DEF_CUSTOM_RENDERED;
    LOG2("subject_distance is %d", mExifAttributes.subject_distance.num);
}

/**
 * Called when the the camera static configuration is known.
 *
 * @arg width: width of the main JPEG picture.
 * @arg height: height of the main JPEG picture.
 */
void EXIFMaker::initialize(int width, int height)
{
    /* We clear the exif attributes, so we won't be using some old values
     * from a previous EXIF generation.
     */
    clear();

    // Initialize the mExifAttributes with specific values
    // time information
    time_t rawtime;
    struct tm *timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    if (timeinfo) {
        strftime((char *)mExifAttributes.date_time, sizeof(mExifAttributes.date_time), "%Y:%m:%d %H:%M:%S", timeinfo);
        // fields: tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst, tm_gmtoff, tm_zone
    } else {
        LOGW("nullptr timeinfo from localtime(), using defaults...");
        struct tm tmpTime = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, "UTC"};
        strftime((char *)mExifAttributes.date_time, sizeof(mExifAttributes.date_time), "%Y:%m:%d %H:%M:%S", &tmpTime);
    }

    // set default subsec time to 1000
    const char subsecTime[] = "1000";
    MEMCPY_S((char *)mExifAttributes.subsec_time, sizeof(mExifAttributes.subsec_time), subsecTime, sizeof(subsecTime));

    // conponents configuration.
    // Default = 4 5 6 0(if RGB uncompressed), 1 2 3 0(other cases)
    // 0 = does not exist; 1 = Y; 2 = Cb; 3 = Cr; 4 = R; 5 = G; 6 = B; other = reserved
    mExifAttributes.components_configuration[0] = 1;
    mExifAttributes.components_configuration[1] = 2;
    mExifAttributes.components_configuration[2] = 3;
    mExifAttributes.components_configuration[3] = 0;

    // set default values for fnumber and focal length
    // (see EXIFMaker::setDriverData() how to override these)
    mExifAttributes.fnumber.num = EXIF_DEF_FNUMBER_NUM;
    mExifAttributes.fnumber.den = EXIF_DEF_FNUMBER_DEN;
    mExifAttributes.focal_length.num = EXIF_DEF_FOCAL_LEN_NUM;
    mExifAttributes.focal_length.den = EXIF_DEF_FOCAL_LEN_DEN;

    // TODO: should ISO be omitted if the value cannot be trusted?
    mExifAttributes.iso_speed_rating = DEFAULT_ISO_SPEED;

    mExifAttributes.aperture.den = EXIF_DEF_APEX_DEN;
    mExifAttributes.aperture.num = EXIF_DEF_APEX_NUM;
    // max aperture. the smallest F number of the lens. unit is APEX value.
    mExifAttributes.max_aperture.num = mExifAttributes.aperture.num;
    mExifAttributes.max_aperture.den = mExifAttributes.aperture.den;

    // subject distance,    0 means distance unknown; (~0) means infinity.
    mExifAttributes.subject_distance.num = EXIF_DEF_SUBJECT_DISTANCE_UNKNOWN;
    mExifAttributes.subject_distance.den = 1;

    // light source, 0 means light source unknown
    mExifAttributes.light_source = 0;
    // TODO: for awb mode


    // gain control, 0 = none;
    // 1 = low gain up; 2 = high gain up; 3 = low gain down; 4 = high gain down
    mExifAttributes.gain_control = 0;

    // contrast, 0 = normal; 1 = soft; 2 = hard; other = reserved
    mExifAttributes.contrast = EXIF_CONTRAST_NORMAL;

    // saturation, 0 = normal; 1 = Low saturation; 2 = High saturation; other = reserved
    mExifAttributes.saturation = EXIF_SATURATION_NORMAL;

    // sharpness, 0 = normal; 1 = soft; 2 = hard; other = reserved
    mExifAttributes.sharpness = EXIF_SHARPNESS_NORMAL;

    // the picture's width and height
    mExifAttributes.width = width;
    mExifAttributes.height = height;

    mExifAttributes.orientation = 1;

    mExifAttributes.custom_rendered = EXIF_DEF_CUSTOM_RENDERED;

    // metering mode, 0 = normal; 1 = soft; 2 = hard; other = reserved
    mExifAttributes.metering_mode = EXIF_METERING_UNKNOWN;
    mInitialized = true;
}

void EXIFMaker::initializeLocation(ExifMetaData* metadata)
{
    LOG1("@%s", __FUNCTION__);
    // GIS information
    bool gpsEnabled = false;
    double latitude = metadata->mGpsSetting.latitude;
    double longitude = metadata->mGpsSetting.longitude;
    double altitude = metadata->mGpsSetting.altitude;
    long timestamp = metadata->mGpsSetting.gpsTimeStamp;
    char* pprocmethod = metadata->mGpsSetting.gpsProcessingMethod;


    //todo: no intel extension for direction, support in future
    /*
    const char *pimgdirection = params.get(IntelCameraParameters::KEY_GPS_IMG_DIRECTION);
    const char *pimgdirectionref = params.get(IntelCameraParameters::KEY_GPS_IMG_DIRECTION_REF);

    double imgdirection = 0.0;
    if (pimgdirection) {
        imgdirection = atof(pimgdirection);
        if ((pimgdirectionref == nullptr)
            ||((strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_TRUE) != 0)
            && (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_MAGNETIC) != 0))
            || (imgdirection < 0 || imgdirection > 359.99))
            pimgdirection = nullptr;
    }
     */
    // check whether the GIS Information is valid
    if(!(latitude >= -EPSILON && latitude <= EPSILON) ||
       !(longitude >= -EPSILON && longitude <= EPSILON) ||
       !(altitude >= -EPSILON && altitude <= EPSILON) ||
       (timestamp != 0) || (strlen(pprocmethod) != 0))
        gpsEnabled = true;

    mExifAttributes.enableGps = 0;
    LOG1("EXIF: gpsEnabled: %d", gpsEnabled);

    // the version is given as 2.2.0.0, it is mandatory when GPSInfo tag is present
    if(gpsEnabled) {
        const unsigned char gpsversion[4] = {0x02, 0x02, 0x00, 0x00};
        MEMCPY_S(mExifAttributes.gps_version_id,
             sizeof(mExifAttributes.gps_version_id),
             gpsversion,
             sizeof(gpsversion));
    } else {
        return;
    }

    // latitude, for example, 39.904214 degrees, N
    if(latitude > 0)
        MEMCPY_S(mExifAttributes.gps_latitude_ref, sizeof(mExifAttributes.gps_latitude_ref),
                 "N", sizeof(mExifAttributes.gps_latitude_ref));
    else
        MEMCPY_S(mExifAttributes.gps_latitude_ref, sizeof(mExifAttributes.gps_latitude_ref),
                 "S", sizeof(mExifAttributes.gps_latitude_ref));

    latitude = fabs(latitude);
    mExifAttributes.gps_latitude[0].num = (uint32_t)latitude;
    mExifAttributes.gps_latitude[0].den = 1;
    mExifAttributes.gps_latitude[1].num = (uint32_t)((latitude - mExifAttributes.gps_latitude[0].num) * 60);
    mExifAttributes.gps_latitude[1].den = 1;
    mExifAttributes.gps_latitude[2].num = (uint32_t)(((latitude - mExifAttributes.gps_latitude[0].num) * 60 - mExifAttributes.gps_latitude[1].num) * 60 * 100);
    mExifAttributes.gps_latitude[2].den = 100;
    mExifAttributes.enableGps |= EXIF_GPS_LATITUDE;
    LOG1("EXIF: latitude, ref:%s, dd:%d, mm:%d, ss:%d",
         mExifAttributes.gps_latitude_ref, mExifAttributes.gps_latitude[0].num,
         mExifAttributes.gps_latitude[1].num, mExifAttributes.gps_latitude[2].num);

    // longitude, for example, 116.407413 degrees, E
    if(longitude > 0)
        MEMCPY_S(mExifAttributes.gps_longitude_ref, sizeof(mExifAttributes.gps_longitude_ref),
                 "E", sizeof(mExifAttributes.gps_longitude_ref));
    else
        MEMCPY_S(mExifAttributes.gps_longitude_ref, sizeof(mExifAttributes.gps_longitude_ref),
                 "W", sizeof(mExifAttributes.gps_longitude_ref));
    longitude = fabs(longitude);
    mExifAttributes.gps_longitude[0].num = (uint32_t)longitude;
    mExifAttributes.gps_longitude[0].den = 1;
    mExifAttributes.gps_longitude[1].num = (uint32_t)((longitude - mExifAttributes.gps_longitude[0].num) * 60);
    mExifAttributes.gps_longitude[1].den = 1;
    mExifAttributes.gps_longitude[2].num = (uint32_t)(((longitude - mExifAttributes.gps_longitude[0].num) * 60 - mExifAttributes.gps_longitude[1].num) * 60 * 100);
    mExifAttributes.gps_longitude[2].den = 100;
    mExifAttributes.enableGps |= EXIF_GPS_LONGITUDE;
    LOG1("EXIF: longitude, ref:%s, dd:%d, mm:%d, ss:%d",
         mExifAttributes.gps_longitude_ref, mExifAttributes.gps_longitude[0].num,
         mExifAttributes.gps_longitude[1].num, mExifAttributes.gps_longitude[2].num);

    // altitude
    // altitude, sea level or above sea level, set it to 0; below sea level, set it to 1
    mExifAttributes.gps_altitude_ref = ((altitude > 0) ? 0 : 1);
    altitude = fabs(altitude);
    mExifAttributes.gps_altitude.num = (uint32_t)altitude;
    mExifAttributes.gps_altitude.den = 1;
    mExifAttributes.enableGps |= EXIF_GPS_ALTITUDE;
    LOG1("EXIF: altitude, ref:%d, height:%d", mExifAttributes.gps_altitude_ref, mExifAttributes.gps_altitude.num);

    // timestamp
    if (timestamp >= LONG_MAX || timestamp <= LONG_MIN)
    {
        timestamp = 0;
        LOGW("invalid timestamp was provided, defaulting to 0 (i.e. 1970)");
    }
    struct tm time;
    gmtime_r(&timestamp, &time);
    time.tm_year += 1900;
    time.tm_mon += 1;
    mExifAttributes.gps_timestamp[0].num = time.tm_hour;
    mExifAttributes.gps_timestamp[0].den = 1;
    mExifAttributes.gps_timestamp[1].num = time.tm_min;
    mExifAttributes.gps_timestamp[1].den = 1;
    mExifAttributes.gps_timestamp[2].num = time.tm_sec;
    mExifAttributes.gps_timestamp[2].den = 1;
    mExifAttributes.enableGps |= EXIF_GPS_TIMESTAMP;

    snprintf((char *)mExifAttributes.gps_datestamp, sizeof(mExifAttributes.gps_datestamp), "%04d:%02d:%02d",
                time.tm_year, time.tm_mon, time.tm_mday);

    LOG1("EXIF: timestamp, year:%d,mon:%d,day:%d,hour:%d,min:%d,sec:%d",
            time.tm_year, time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);

    // processing method
    MEMCPY_S(mExifAttributes.gps_processing_method,
             sizeof(mExifAttributes.gps_processing_method),
             metadata->mGpsSetting.gpsProcessingMethod,
             sizeof(metadata->mGpsSetting.gpsProcessingMethod));
    mExifAttributes.gps_processing_method[sizeof(mExifAttributes.gps_processing_method) - 1] = 0;

    mExifAttributes.enableGps |= EXIF_GPS_PROCMETHOD;
    LOG1("EXIF: GPS processing method:%s", mExifAttributes.gps_processing_method);

    //todo: no intel extension for direction, support in future
    /*
    if (pimgdirection && pimgdirectionref)  {
        if (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_TRUE) == 0)
            memcpy(mExifAttributes.gps_img_direction_ref, "T", sizeof(mExifAttributes.gps_img_direction_ref));
        else if (strcmp(pimgdirectionref, IntelCameraParameters::GPS_IMG_DIRECTION_REF_MAGNETIC) == 0)
            memcpy(mExifAttributes.gps_img_direction_ref, "M", sizeof(mExifAttributes.gps_img_direction_ref));
        mExifAttributes.gps_img_direction.num = (uint32_t)(imgdirection*100);
        mExifAttributes.gps_img_direction.den = 100;
        mExifAttributes.enableGps |= EXIF_GPS_IMG_DIRECTION;
        LOG1("EXIF: GPS img direction ref:%s, img direction num:%d, den:%d",
            mExifAttributes.gps_img_direction_ref,
            mExifAttributes.gps_img_direction.num,
            mExifAttributes.gps_img_direction.den);
    }
    */
}

void EXIFMaker::setSensorAeConfig()
{
    LOG1("@%s", __FUNCTION__);

    mExifAttributes.exposure_time.num = 0;
    mExifAttributes.exposure_time.den = 1;
    mExifAttributes.shutter_speed.num = 0;
    mExifAttributes.shutter_speed.den = 1;
    mExifAttributes.exposure_bias.num = 0;
    mExifAttributes.exposure_bias.den = 100;
}

/*
 * more secure attribute copy routine.
 * \param dst pointer to dst buffer
 * \param dstSize dst buffer size
 * \param src pointer to src character buffer
 * \param srcLength src buffer length in characters, not including null byte
 */
void EXIFMaker::copyAttribute(uint8_t *dst, size_t dstSize,
                              const char *src, size_t srcLength)
{
    size_t dstMaxLength = dstSize - 1; // leave space for null
    MEMCPY_S(dst, dstMaxLength, src, srcLength); // copy chars (not null)
    // add null termination
    size_t len = std::min(dstMaxLength, srcLength);
    dst[len] = '\0';
}

void EXIFMaker::clear()
{
    LOG1("@%s", __FUNCTION__);
    // Reset all the attributes
    CLEAR(mExifAttributes);
    // Initialize the common values
    mExifAttributes.enableThumb = false;
    copyAttribute(mExifAttributes.image_description,
                  sizeof(mExifAttributes.image_description),
                  EXIF_DEF_IMAGE_DESCRIPTION,
                  strlen(EXIF_DEF_IMAGE_DESCRIPTION));

    copyAttribute(mExifAttributes.maker,
                  sizeof(mExifAttributes.maker),
                  "INTEL",
                  strlen("INTEL"));

    copyAttribute(mExifAttributes.model,
                  sizeof(mExifAttributes.model),
                  "Chrome",
                  strlen("Chrome"));

    copyAttribute(mExifAttributes.software,
                  sizeof(mExifAttributes.software),
                  EXIF_DEF_SOFTWARE,
                  strlen(EXIF_DEF_SOFTWARE));

    copyAttribute(mExifAttributes.exif_version,
                  sizeof(mExifAttributes.exif_version),
                  EXIF_DEF_EXIF_VERSION,
                  strlen(EXIF_DEF_EXIF_VERSION));

    copyAttribute(mExifAttributes.flashpix_version,
                  sizeof(mExifAttributes.flashpix_version),
                  EXIF_DEF_FLASHPIXVERSION,
                  strlen(EXIF_DEF_FLASHPIXVERSION));

    // initially, set default flash
    mExifAttributes.flash = EXIF_DEF_FLASH;

    // normally it is sRGB, 1 means sRGB. FFFF.H means uncalibrated
    mExifAttributes.color_space = EXIF_DEF_COLOR_SPACE;

    // the number of pixels per ResolutionUnit in the w or h direction
    // 72 means the image resolution is unknown
    mExifAttributes.x_resolution.num = EXIF_DEF_RESOLUTION_NUM;
    mExifAttributes.x_resolution.den = EXIF_DEF_RESOLUTION_DEN;
    mExifAttributes.y_resolution.num = mExifAttributes.x_resolution.num;
    mExifAttributes.y_resolution.den = mExifAttributes.x_resolution.den;
    // resolution unit, 2 means inch
    mExifAttributes.resolution_unit = EXIF_DEF_RESOLUTION_UNIT;
    // when thumbnail uses JPEG compression, this tag 103H's value is set to 6
    mExifAttributes.compression_scheme = EXIF_DEF_COMPRESSION;

    // the TIFF default is 1 (centered)
    mExifAttributes.ycbcr_positioning = EXIF_DEF_YCBCR_POSITIONING;

    // Clear the Intel 3A Makernote information
    mExifAttributes.makerNoteData = mMakernoteSection;
    mExifAttributes.makerNoteDataSize = 0;
    mExifAttributes.makernoteToApp2 = false;

    mInitialized = false;
}

void EXIFMaker::enableFlash(bool enable, int8_t aeMode, int8_t flashMode)
{

    mExifAttributes.flash = EXIF_DEF_FLASH;

}

void EXIFMaker::setThumbnail(unsigned char *data, size_t size, int width, int height)
{
    LOG1("@%s: data = %p, size = %zu", __FUNCTION__, data, size);
    mExifAttributes.enableThumb = true;
    mExifAttributes.widthThumb = width;
    mExifAttributes.heightThumb = height;
    if (mEncoder.setThumbData(data, size) != EXIF_SUCCESS) {
        LOGE("Error in setting EXIF thumbnail");
    }
}

bool EXIFMaker::isThumbnailSet() const {
    LOG1("@%s", __FUNCTION__);
    return mEncoder.isThumbDataSet();
}

size_t EXIFMaker::makeExif(unsigned char **data)
{
    LOG1("@%s", __FUNCTION__);
    if (*data == nullptr) {
        LOGE("nullptr passed for EXIF. Cannot generate EXIF!");
        return 0;
    }
    if (mEncoder.makeExif(*data, &mExifAttributes, &mExifSize) == EXIF_SUCCESS) {
        LOG1("Generated EXIF (@%p) of size: %zu", *data, mExifSize);
        return mExifSize;
    }
    return 0;
}

void EXIFMaker::setMaker(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(mExifAttributes.maker));
    strncpy((char*)mExifAttributes.maker, data, len-1);
    mExifAttributes.maker[len-1] = '\0';
}

void EXIFMaker::setModel(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(mExifAttributes.model));
    strncpy((char*)mExifAttributes.model, data, len-1);
    mExifAttributes.model[len-1] = '\0';
}

void EXIFMaker::setSoftware(const char *data)
{
    LOG1("@%s: data = %s", __FUNCTION__, data);
    size_t len(sizeof(mExifAttributes.software));
    strncpy((char*)mExifAttributes.software, data, len-1);
    mExifAttributes.software[len-1] = '\0';
}

void EXIFMaker::saveMakernote(const Parameters *params)
{
    Check(params == nullptr, VOID_VALUE, "params is nullptr");
    unsigned int size = sizeof(unsigned char) * (MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE);
    if (params->getMakernoteData(mMakernoteSection, &size) == OK) {
        mExifAttributes.makerNoteDataSize = size;
    }
}

void EXIFMaker::updateSensorInfo(const Parameters *params)
{
    float focal = 0.0;
    params->getFocalLength(focal);

    if (focal < EPSILON) {
        // Focal length is not supported, set to default value
        icamera::CameraMetadata meta;
        icamera::ParameterHelper::copyMetadata(*params, &meta);

        icamera_metadata_entry entry = meta.find(CAMERA_LENS_INFO_AVAILABLE_FOCAL_LENGTHS);
        if (entry.count >= 1) {
            focal = entry.data.f[0];
        }
    }

    LOG2("focal length is %f", focal);
    mExifAttributes.focal_length.num = focal * mExifAttributes.focal_length.den;
    float aperture = 0.0;
    params->getAperture(aperture);
    mExifAttributes.aperture.num = aperture * mExifAttributes.aperture.den;
}

} // namespace icamera
