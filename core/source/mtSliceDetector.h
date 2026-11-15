#ifndef SOURCE_MTSLICEDETECTOR_H_
#define SOURCE_MTSLICEDETECTOR_H_

#include "mtStructs.h"

constexpr uint16_t LOCAL_BUFFER_SIZE = 10240;  // Rozmiar lokalnego buforu tła
constexpr uint16_t INSTANT_BUFFER_SIZE = 2048; // Rozmiar buforu chwilowego

// Po wykryciu peaku często w kolejnych buforach wykrywane są kolejne peaki, ponieważ średnia
// energetyczna nie zdąży wzrosnąć w kolejnych kilku buforach. Wysokoenergetyczne bufory po peaku
// muszą zastąpić niskoenergetyczne bufory tła, aby kolejne wysoko energetyczne bufory nie
// wyróżniały się w odniesieniu do tła, ma to swoją bezwładność określoną w zmiennej
// LAST_DETECTED_DEATH_ZONE
constexpr uint16_t LAST_DETECTED_DEATH_ZONE = LOCAL_BUFFER_SIZE;

// rozmiar bufora dokładnego wyznaczania peaku - oznacza jednocześnie dokładność wyznaczania, ale
// nie można z nią przesadzić ponieważ uśrednianie potrzebuje rozsądnej ilości próbek ( wygładzanie
// statystyczne)
constexpr uint16_t PRECISE_INSTANT_BUFFER_SIZE = 128;
constexpr uint16_t PRECISE_LOCAL_BUFFER_SIZE = INSTANT_BUFFER_SIZE * 6;
extern uint32_t measureBuffer[INSTANT_BUFFER_SIZE];

class mtSliceDetector
{
  public:
    void start(int16_t* addr, uint32_t len, uint16_t* sliceTab, uint8_t* sliceNumber);
    void update();
    uint8_t getState();
    uint8_t getProgress();
    static float mpdjSensScale; // MPDJ (5g-2): threshold scale, < 1 = more slices

  private:
    uint32_t preciseDetect(uint32_t position);
    uint32_t calcInstantAverage(uint32_t* buf, uint16_t size);
    uint32_t calcLocalAverage(uint32_t* buf, uint16_t size);
    uint32_t calcLocalVariance(uint32_t* buf, uint32_t average, uint16_t size);
    enum struct detectState
    {
        ended,
        inProgress
    };

    detectState state = detectState::ended;
    uint32_t position = 0;           // pozycja w samplu
    uint8_t averagePositionHead = 0; // pozycja aktualnego bufora chwilowego
    int16_t* address = nullptr;      // adres sampla
    uint16_t* slice =
      nullptr; // adres tablicy sliców, która jest tablicą wskaźników do adresów w samplu
    uint32_t length = 0;                         // dlugosc sampla
    uint32_t* const measureBuff = measureBuffer; // stały wskaźnik do bufora w externalRam ( żeby
                                                 // zachowywał się tak samo jak adres tablicy)
    uint32_t
      instantAverageBuffer[LOCAL_BUFFER_SIZE / INSTANT_BUFFER_SIZE]; // tablica średnich (akumulacja
                                                                     // srednich buforow chwilowych)
    uint32_t localAverage;          // srednia lokalna
    uint32_t currentInstantAverage; // bierząca średnia lokalna
    uint32_t localVariance;         // lokalna wariancja
    uint32_t lastDetected;          // ostatnio wykryty slice
    uint8_t lastBufferFlag;         // flaga ostatniego bufora
    float sensCoeff;                // współczynnik czułośc wykrywania
    uint8_t currentSlice = 0;       // licznik slicow
    uint8_t* sliceNumberToWrite; // wskaznik do zmiennej do której zostanie wpisana liczba slajsów
};

extern mtSliceDetector sliceDetector;

#endif /* SOURCE_MTSLICEDETECTOR_H_ */
