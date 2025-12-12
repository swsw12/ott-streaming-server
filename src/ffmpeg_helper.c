/*
 * OTT Video Streaming Server - FFmpeg Helper
 * Thumbnail extraction and video duration detection
 */

#include "common.h"

/* Check if FFmpeg is available */
int ffmpeg_check_available(void) {
#if defined(_WIN32)
    int result = system(".\\ffmpeg.exe -version >nul 2>&1");
#else
    int result = system("ffmpeg -version >/dev/null 2>&1");
#endif
    return result == 0;
}

/* Extract thumbnail from video at specific timestamp */
int ffmpeg_extract_thumbnail(const char* video_path, const char* output_path, int timestamp_sec) {
    char command[1024];
    
#if defined(_WIN32)
    snprintf(command, sizeof(command),
        ".\\ffmpeg.exe -y -ss %d -i \"%s\" -frames:v 1 -q:v 2 \"%s\" >nul 2>&1",
        timestamp_sec, video_path, output_path);
#else
    snprintf(command, sizeof(command),
        "ffmpeg -y -ss %d -i \"%s\" -frames:v 1 -q:v 2 \"%s\" >/dev/null 2>&1",
        timestamp_sec, video_path, output_path);
#endif
    
    log_message(LOG_DEBUG, "Extracting thumbnail: %s", command);
    
    int result = system(command);
    
    if (result != 0) {
        log_message(LOG_WARN, "Failed to extract thumbnail from %s", video_path);
        return -1;
    }
    
    log_message(LOG_INFO, "Thumbnail extracted: %s", output_path);
    return 0;
}

/* Get video duration in seconds using FFprobe */
int ffmpeg_get_duration(const char* video_path) {
    char command[1024];
    char duration_file[256];
    
#if defined(_WIN32)
    snprintf(duration_file, sizeof(duration_file), "%s\\duration_tmp.txt", DATA_DIR);
    snprintf(command, sizeof(command),
        ".\\ffprobe.exe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\" > \"%s\" 2>nul",
        video_path, duration_file);
#else
    snprintf(duration_file, sizeof(duration_file), "%s/duration_tmp.txt", DATA_DIR);
    snprintf(command, sizeof(command),
        "ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 \"%s\" > \"%s\" 2>/dev/null",
        video_path, duration_file);
#endif
    
    int result = system(command);
    
    if (result != 0) {
        log_message(LOG_WARN, "Failed to get duration for %s", video_path);
        return 0;
    }
    
    /* Read duration from temp file */
    FILE* fp = fopen(duration_file, "r");
    if (!fp) return 0;
    
    double duration = 0;
    if (fscanf(fp, "%lf", &duration) != 1) {
        duration = 0;
    }
    fclose(fp);
    
    /* Clean up temp file */
    remove(duration_file);
    
    return (int)duration;
}

/* Scan video directory and generate thumbnails */
int ffmpeg_scan_videos(void) {
    int count = 0;
    char video_path[MAX_PATH_LEN];
    char thumb_path[MAX_PATH_LEN];
    int has_ffmpeg = ffmpeg_check_available();
    
    log_message(LOG_INFO, "Scanning video directory: %s", VIDEO_DIR);
    if (!has_ffmpeg) {
        log_message(LOG_WARN, "FFmpeg not found - registering videos without thumbnails");
    }
    
#if defined(_WIN32)
    WIN32_FIND_DATAA fd;
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, sizeof(search_path), "%s\\*.mp4", VIDEO_DIR);
    
    HANDLE hFind = FindFirstFileA(search_path, &fd);
    log_message(LOG_INFO, "Searching for videos in: %s", search_path);
    
    if (hFind == INVALID_HANDLE_VALUE) {
        log_message(LOG_WARN, "No MP4 files found in %s (Error: %d)", VIDEO_DIR, GetLastError());
        return 0;
    }
    
    do {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        
        snprintf(video_path, sizeof(video_path), "%s\\%s", VIDEO_DIR, fd.cFileName);
        log_message(LOG_INFO, "Found file: %s", fd.cFileName);
        
        /* Generate thumbnail filename */
        char* ext = strrchr(fd.cFileName, '.');
        char basename[256];
        if (ext) {
            size_t len = ext - fd.cFileName;
            strncpy(basename, fd.cFileName, len);
            basename[len] = '\0';
        } else {
            strcpy(basename, fd.cFileName);
        }
        
        snprintf(thumb_path, sizeof(thumb_path), "%s\\%s.jpg", THUMBNAIL_DIR, basename);
        
        /* Try to extract thumbnail if FFmpeg available */
        if (has_ffmpeg && GetFileAttributesA(thumb_path) == INVALID_FILE_ATTRIBUTES) {
            ffmpeg_extract_thumbnail(video_path, thumb_path, 10);
        }
        
        /* Get duration (0 if no FFmpeg) */
        int duration = has_ffmpeg ? ffmpeg_get_duration(video_path) : 0;
        
        /* Check if video already in database */
        Video* existing = NULL;
        Video* videos;
        int video_count = video_get_all(&videos);
        for (int i = 0; i < video_count; i++) {
            if (strcmp(videos[i].filename, fd.cFileName) == 0) {
                existing = &videos[i];
                break;
            }
        }
        
        if (!existing) {
            /* Create title from filename */
            char title[256];
            strcpy(title, basename);
            /* Replace underscores with spaces */
            for (char* p = title; *p; p++) {
                if (*p == '_') *p = ' ';
            }
            
            char thumb_relative[256];
            snprintf(thumb_relative, sizeof(thumb_relative), "thumbnails/%s.jpg", basename);
            
            video_add(title, fd.cFileName, thumb_relative, duration, "");
            count++;
            log_message(LOG_INFO, "Added video: %s (duration: %d sec)", title, duration);
        }
        
    } while (FindNextFileA(hFind, &fd));
    
    FindClose(hFind);
#else
    DIR* dir = opendir(VIDEO_DIR);
    if (!dir) {
        log_message(LOG_WARN, "Cannot open video directory: %s", VIDEO_DIR);
        return 0;
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Check for .mp4 extension */
        char* ext = strrchr(entry->d_name, '.');
        if (!ext || strcasecmp(ext, ".mp4") != 0) continue;
        
        snprintf(video_path, sizeof(video_path), "%s/%s", VIDEO_DIR, entry->d_name);
        
        /* Generate thumbnail filename */
        char basename[256];
        size_t len = ext - entry->d_name;
        strncpy(basename, entry->d_name, len);
        basename[len] = '\0';
        
        snprintf(thumb_path, sizeof(thumb_path), "%s/%s.jpg", THUMBNAIL_DIR, basename);
        
        /* Check if thumbnail already exists */
        struct stat st;
        if (stat(thumb_path, &st) != 0) {
            /* Extract thumbnail at 10 seconds */
            ffmpeg_extract_thumbnail(video_path, thumb_path, 10);
        }
        
        /* Get duration */
        int duration = ffmpeg_get_duration(video_path);
        
        /* Check if video already in database */
        Video* existing = NULL;
        Video* videos;
        int video_count = video_get_all(&videos);
        for (int i = 0; i < video_count; i++) {
            if (strcmp(videos[i].filename, entry->d_name) == 0) {
                existing = &videos[i];
                break;
            }
        }
        
        if (!existing) {
            /* Create title from filename */
            char title[256];
            strcpy(title, basename);
            /* Replace underscores with spaces */
            for (char* p = title; *p; p++) {
                if (*p == '_') *p = ' ';
            }
            
            char thumb_relative[256];
            snprintf(thumb_relative, sizeof(thumb_relative), "thumbnails/%s.jpg", basename);
            
            video_add(title, entry->d_name, thumb_relative, duration, "");
            count++;
            log_message(LOG_INFO, "Added video: %s (duration: %d sec)", title, duration);
        }
    }
    
    closedir(dir);
#endif
    
    log_message(LOG_INFO, "Scan complete. Added %d new videos.", count);
    return count;
}

/* Create default thumbnail if FFmpeg fails */
void ffmpeg_create_default_thumbnail(const char* output_path) {
    /* Create a simple placeholder image - just copy a default if exists */
    const char* default_thumb = STATIC_DIR "/default_thumbnail.jpg";
    
#if defined(_WIN32)
    CopyFileA(default_thumb, output_path, FALSE);
#else
    char command[512];
    snprintf(command, sizeof(command), "cp \"%s\" \"%s\" 2>/dev/null", default_thumb, output_path);
    system(command);
#endif
}
