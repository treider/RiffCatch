ASIO SDK (Steinberg) must be downloaded separately and extracted to third_party/asio_sdk/. It is gitignored; do not commit it.
Or pass -DASIO_SDK_DIR=<path> to CMake.

LAME (LGPL) MP3 encoder: extract the official lame-3.100 source to third_party/lame/ (gitignored), or pass -DLAME_DIR=<path>. Without it the app builds without MP3 export.
