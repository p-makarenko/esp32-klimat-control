#include "global_declarations.h" // Додайте цей include

struct SensorData {
    float tempRoom;        // Температура кімнати (DS18B20)
    float tempCarrier;     // Температура теплоносія (DS18B20)
    float tempBME;         // Температура BME280
    float humidity;        // Вологість BME280
    float pressure;        // Тиск BME280
    float bmeOffset;       // Різниця між DS18B20 і BME280 (додайте це поле!)
    bool roomValid;        // Валідність датчика кімнати
    bool carrierValid;     // Валідність датчика теплоносія
    bool bmeValid;         // Валідність BME280
    unsigned long timestamp;
};