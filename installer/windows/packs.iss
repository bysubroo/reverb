; Measured location packs downloaded by the installers (Windows: SubRooRooms.iss, macOS: build_pkg.sh).
; The archive is AES-256 encrypted 7z, published as a release of github.com/bysubroo/subroo-rooms-content.
; To ship new packs: build a new archive, push it to the content repo as packs/<version>, then bump these values.
#define PackVersion "2"
#define PackFile "SubRooRooms-Packs-v2.7z"
#define PackBaseUrl "https://github.com/bysubroo/subroo-rooms-content/releases/download/packs-v2/"
#define PackUrl PackBaseUrl + PackFile
#define PackSha256Value "dcacc0962b456814a900233ba6b3817c35559a58159d6fcd4f3462787cb50205"
#define PackUnpackedSize "324880550"
#define PackDownloadMB "133"
#define PackCount "221"

; The key comes from the build (/DPackKey=...). Without it the installer is built without packs.
#ifdef PackKey
  #define PackSha256 PackSha256Value
#endif
