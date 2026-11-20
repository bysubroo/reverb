; Measured location packs downloaded by the installers (Windows: SubRooRooms.iss, macOS: build_pkg.sh).
; The archive is AES-256 encrypted 7z, published as a release of github.com/bysubroo/subroo-rooms-content.
; To ship new packs: build a new archive, push it to the content repo as packs/<version>, then bump these values.
#define PackVersion "3"
#define PackFile "SubRooRooms-Packs-v3.7z"
#define PackBaseUrl "https://github.com/bysubroo/subroo-rooms-content/releases/download/packs-v3/"
#define PackUrl PackBaseUrl + PackFile
#define PackSha256Value "fe98b27ee780f4986fe87e2a1b985d9b26190fddeb9ac8e2da506a418d22208e"
#define PackUnpackedSize "324880550"
#define PackDownloadMB "133"
#define PackCount "221"

; The key comes from the build (/DPackKey=...). Without it the installer is built without packs.
#ifdef PackKey
  #define PackSha256 PackSha256Value
#endif
