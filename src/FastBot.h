/*
    Очень простая и быстрая библиотека для телеграм бота
    Документация: 
    GitHub: https://github.com/GyverLibs/FastBot
    Возможности:
    - Оптимизирована для большой нагрузки (спокойно принимает 50 сообщ в секунду)
    - Опциональная установка ID чата для общения с ботом
    - Проверка обновлений вручную или по таймеру
    - Сообщения приходят в функцию-обработчик
    - Отправка сообщений в чат
    - Вывод меню вместо клавиатуры
    - Вывод инлайн меню в сообщении
    - Возможность включить ручной инкремент новых сообщений
    
    AlexGyver, alex@alexgyver.ru
    https://alexgyver.ru/
    MIT License

    Версии:
    v1.0 - релиз
    v1.1 - оптимизация
*/

/*
Статусы tick:
    0 - ожидание
    1 - ОК
    2 - Переполнен по ovf
    3 - Ошибка телеграм
    4 - Ошибка подключения
*/

#ifndef FastBot_h
#define FastBot_h
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
static std::unique_ptr<BearSSL::WiFiClientSecure>client(new BearSSL::WiFiClientSecure);
static HTTPClient https;

class FastBot {
public:
    // инициализация (токен, макс кол-во сообщений на запрос, макс символов, период)
    FastBot(String token, int limit = 10, int ovf = 10000, int period = 1000) {
        _request = (String)"https://api.telegram.org/bot" + token;
        _ovf = ovf;
        _limit = limit;
        _period = period;
        client->setInsecure();
    }
    
    // установка ID чата для парсинга сообщений
    void setChatID(String chatID) {
        _chatID = chatID;
    }
    void setChatID(const char* chatID) {
        _chatID = chatID;
    }
    
    // подключение обработчика сообщений
    void attach(void (*handler)(String&, String&)) {
        _callback = handler;
    }
    
    // отключение обработчика сообщений
    void detach() {
        _callback = NULL;
    }
    
    // ручная проверка обновлений
    uint8_t tickManual() {
        uint8_t status = 1;
        if (https.begin(*client, _request + "/getUpdates?limit=" + _limit + "&offset=" + ID)) {
            if (https.GET() == HTTP_CODE_OK) status = parse(https.getString());
            else status = 3;
            https.end();
        } else status = 4;
        return status;
    }
    
    // проверка обновлений по таймеру
    uint8_t tick() {
        if (millis() - tmr >= _period) {
            tmr = millis();
            return tickManual();
        }
        return 0;
    }

    // отправить сообщение в чат
    uint8_t sendMessage(const char* msg) {
        String request = _request + "/sendMessage?chat_id=" + _chatID + "&text=" + msg;
        return sendRequest(request);
    }
    uint8_t sendMessage(String& msg) {
        return sendMessage(msg.c_str());
    }

    // показать меню
    uint8_t showMenu(const char* str) {
        String request = _request + "/sendMessage?chat_id=" + _chatID + "&text=Show Menu&reply_markup={\"keyboard\":[[\"";
        for (int i = 0; i < strlen(str); i++) {
            char c = str[i];
            if (c == '\t') request += "\",\"";
            else if (c == '\n') request += "\"],[\"";
            else request += c;
        }
        request += "\"]],\"resize_keyboard\":true}";
        return sendRequest(request);
    }
    uint8_t showMenu(String& str) {
        return showMenu(str.c_str());
    }
    
    // скрыть меню
    uint8_t closeMenu() {
        String request = _request + "/sendMessage?chat_id=" + _chatID + "&text=Close Menu&reply_markup={\"remove_keyboard\":true}";
        return sendRequest(request);
    }
    
    // показать инлайн меню
    uint8_t inlineMenu(const char* msg, const char* str) {
        String request = _request + "/sendMessage?chat_id=" + _chatID + "&text=" + msg + "&reply_markup={\"inline_keyboard\":[[{";
        String buf = "";
        for (int i = 0; i < strlen(str); i++) {
            char c = str[i];
            if (c == '\t') {
                addInlineButton(request, buf);
                request += "},{";
                buf = "";
            }
            else if (c == '\n') {
                addInlineButton(request, buf);
                request += "}],[{";
                buf = "";
            }
            else buf += c;
        }
        addInlineButton(request, buf);
        request += "}]]}";
        return sendRequest(request);
    }

    uint8_t inlineMenu(String& msg, String& str) {
        return inlineMenu(msg.c_str(), str.c_str());
    }
    
    // отправить запрос
    uint8_t sendRequest(String& req) {
        uint8_t status = 1;
        if (https.begin(*client, req)) {
            if (https.GET() != HTTP_CODE_OK) status = 3;
            https.end();
        } else status = 4;
        return status;
    }
    
    // авто инкремент сообщений (по умолч включен)
    void autoIncrement(boolean incr) {
        _incr = incr;
    }
    
    // вручную инкрементировать ID
    void incrementID(uint8_t val) {
        if (_incr) ID += val;
    }

private:
    void addInlineButton(String& str, String& msg) {
        str += "\"text\":\"" + msg + "\",\"callback_data\":\"" + msg + '\"';
    }
    uint8_t parse(const String& str) {
        if (!str.startsWith("{\"ok\":true")) {    // ошибка запроса (неправильный токен итд)
            https.end();
            return 3;
        }
        if (https.getSize() > _ovf) {             // переполнен
            int IDpos = str.indexOf("{\"update_id\":", IDpos);
            if (IDpos > 0) ID = str.substring(IDpos + 13, str.indexOf(',', IDpos)).toInt();
            ID++;
            https.end();
            return 2;
        }

        int IDpos = str.indexOf("{\"update_id\":", 0);  // первая позиция ключа update_id
        int counter = 0;
        while (true) {
            if (IDpos > 0 && IDpos < str.length()) {      // если есть что разбирать
                if (ID == 0) ID = str.substring(IDpos + 13, str.indexOf(',', IDpos)).toInt() + 1;   // холодный запуск, ищем ID
                else counter++;                                                                     // иначе считаем пакеты
                int textPos = IDpos;                                  // стартовая позиция для поиска
                int endPos;
                IDpos = str.indexOf("{\"update_id\":", IDpos + 1);    // позиция id СЛЕДУЮЩЕГО обновления (мы всегда на шаг впереди)
                if (IDpos == -1) IDpos = str.length();                // если конец пакета - для удобства считаем что позиция ID в конце

                // установлена проверка на ID чата - проверяем соответствие
                if (_chatID.length() > 0) {
                    textPos = str.indexOf("\"chat\":{\"id\":", textPos);
                    if (textPos < 0 || textPos > IDpos) continue;
                    endPos = str.indexOf(",\"", textPos);
                    String chatID = str.substring(textPos + 13, endPos);
                    textPos = endPos;
                    if (!chatID.equals(_chatID)) continue;  // не тот чат
                }

                // ищем имя юзера
                textPos = str.indexOf("\"username\":\"", textPos);
                if (textPos < 0 || textPos > IDpos) continue;
                endPos = str.indexOf("\",\"", textPos);
                String name = str.substring(textPos + 12, endPos);

                // ищем сообщение
                String msg;
                int dataPos = str.indexOf("\"data\":", textPos);  // вдруг это callback_data
                if (dataPos > 0 && dataPos < IDpos) {
                    endPos = str.indexOf("\"}}", textPos);
                    msg = str.substring(dataPos + 8, endPos);       // забираем callback_data
                } else {
                    textPos = str.indexOf(",\"text\":\"", textPos);
                    if (textPos < 0 || textPos > IDpos) continue;
                    endPos = str.indexOf("\"}}", textPos);
                    int endPos2 = str.indexOf("\",\"entities", textPos);
                    if (endPos > 0 && endPos2 > 0) endPos = min(endPos, endPos2);
                    else if (endPos < 0) endPos = endPos2;
                    if (str[textPos + 9] == '/') textPos++;
                    msg = str.substring(textPos + 9, endPos);       // забираем обычное сообщение
                }
                if (*_callback) _callback(name, msg);

            } else break;   // IDpos > 0
        }
        if (_incr) ID += counter;
        return 1;
    }

    void (*_callback)(String& name, String& msg) = NULL;
    String _request;
    int _ovf = 5000, _period = 1000, _limit = 10;
    long ID = 0;
    uint32_t tmr = 0;
    String _chatID = "";
    boolean _incr = true;
};
#endif
