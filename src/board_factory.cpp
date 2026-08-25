#include "board/board_factory.h"

#if defined(JARVIS_BOARD_FUTUREPROOFHOMES_SATELLITE1_1)
#include <satellite1_1_board.h>
Board& selectedBoard() {
    static FutureProofHomesSatellite11Board board;
    return board;
}
#elif defined(JARVIS_BOARD_WAVESHARE_185C)
#include <waveshare_185c_board.h>
Board& selectedBoard() {
    static Waveshare185CBoard board;
    return board;
}
#elif defined(JARVIS_BOARD_GENERIC_ESP32S3)
#include <generic_esp32s3_board.h>
Board& selectedBoard() {
    static GenericEsp32S3Board board;
    return board;
}
#else
#error "Kein Board-Profil gewählt. PlatformIO environment verwenden."
#endif
