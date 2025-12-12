# OTT Video Streaming Server Makefile

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g

# Platform detection
ifeq ($(OS),Windows_NT)
    TARGET = ott_server.exe
    LDFLAGS = -lws2_32
    RM = del /Q
    MKDIR = if not exist "$(1)" mkdir "$(1)"
else
    TARGET = ott_server
    LDFLAGS = -lpthread
    RM = rm -f
    MKDIR = mkdir -p $(1)
endif

# Source files
SRCS = src/main.c src/utils.c src/data.c src/ffmpeg_helper.c src/http_handler.c
OBJS = $(SRCS:.c=.o)

# Default target
all: dirs $(TARGET)

# Create directories
dirs:
ifeq ($(OS),Windows_NT)
	@if not exist "data" mkdir data
	@if not exist "videos" mkdir videos
	@if not exist "static\thumbnails" mkdir "static\thumbnails"
else
	@mkdir -p data videos static/thumbnails
endif

# Build target
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	@echo "Build complete: $(TARGET)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build files
clean:
ifeq ($(OS),Windows_NT)
	@if exist src\*.o del /Q src\*.o
	@if exist $(TARGET) del /Q $(TARGET)
else
	$(RM) src/*.o $(TARGET)
endif
	@echo "Clean complete"

# Run server
run: all
	./$(TARGET)

# Install (Linux only)
install: all
ifeq ($(OS),Windows_NT)
	@echo "Install not supported on Windows"
else
	install -m 755 $(TARGET) /usr/local/bin/
endif

# Create sample video (requires ffmpeg)
sample:
ifeq ($(OS),Windows_NT)
	@echo Creating sample video...
	ffmpeg -y -f lavfi -i testsrc=duration=60:size=1280x720:rate=30 -f lavfi -i sine=frequency=440:duration=60 -c:v libx264 -c:a aac videos/sample_video.mp4 2>nul
	@echo Sample video created: videos/sample_video.mp4
else
	@echo "Creating sample video..."
	ffmpeg -y -f lavfi -i testsrc=duration=60:size=1280x720:rate=30 -f lavfi -i sine=frequency=440:duration=60 -c:v libx264 -c:a aac videos/sample_video.mp4 2>/dev/null
	@echo "Sample video created: videos/sample_video.mp4"
endif

# Help
help:
	@echo "OTT Video Streaming Server"
	@echo ""
	@echo "Usage:"
	@echo "  make          - Build the server"
	@echo "  make clean    - Remove build files"
	@echo "  make run      - Build and run the server"
	@echo "  make sample   - Create a sample test video (requires ffmpeg)"
	@echo "  make help     - Show this help"
	@echo ""
	@echo "After building, run: ./$(TARGET) [port]"
	@echo "Default port is 8080"

.PHONY: all dirs clean run install sample help
