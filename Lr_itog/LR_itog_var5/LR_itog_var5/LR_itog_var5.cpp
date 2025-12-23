#include "json.hpp"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <random>
#include <ctime>
#include <map>

using namespace std;
using json = nlohmann::json;

// ==================== СТРУКТУРЫ ДАННЫХ ====================

struct DateResult {
    string raw_date;        // Исходная дата
    string iso_date;        // Дата в формате ISO (YYYY-MM-DD)
    string converted_date;  // Сконвертированная дата
    string error_reason;    // Причина ошибки
    bool valid;             // Валидность даты
};

// ==================== КЛАСС КОНВЕРТЕРА ДАТ ====================

class DateConverter {
private:
    map<string, string> month_map = {
        // Русские полные
        {"января", "01"}, {"февраля", "02"}, {"марта", "03"}, {"апреля", "04"},
        {"мая", "05"}, {"июня", "06"}, {"июля", "07"}, {"августа", "08"},
        {"сентября", "09"}, {"октября", "10"}, {"ноября", "11"}, {"декабря", "12"},
        // Русские короткие
        {"янв", "01"}, {"фев", "02"}, {"мар", "03"}, {"апр", "04"},
        {"май", "05"}, {"июн", "06"}, {"июл", "07"}, {"авг", "08"},
        {"сен", "09"}, {"окт", "10"}, {"ноя", "11"}, {"дек", "12"},
        // Английские полные
        {"january", "01"}, {"february", "02"}, {"march", "03"}, {"april", "04"},
        {"may", "05"}, {"june", "06"}, {"july", "07"}, {"august", "08"},
        {"september", "09"}, {"october", "10"}, {"november", "11"}, {"december", "12"},
        // Английские короткие
        {"jan", "01"}, {"feb", "02"}, {"mar", "03"}, {"apr", "04"},
        {"may", "05"}, {"jun", "06"}, {"jul", "07"}, {"aug", "08"},
        {"sep", "09"}, {"oct", "10"}, {"nov", "11"}, {"dec", "12"}
    };

    // Проверка високосного года
    bool isLeapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }

    // Валидация даты (обработка високосных лет)
    bool validateDate(int year, int month, int day) {
        if (year < 1900 || year > 2100) return false;
        if (month < 1 || month > 12) return false;

        int days_in_month[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        if (month == 2 && isLeapYear(year)) {
            days_in_month[1] = 29;
        }

        return day >= 1 && day <= days_in_month[month - 1];
    }

    // Парсинг ISO формата (YYYY-MM-DD)
    bool parseISO(const string& date_str, int& year, int& month, int& day) {
        if (date_str.length() != 10 || date_str[4] != '-' || date_str[7] != '-') {
            return false;
        }

        try {
            year = stoi(date_str.substr(0, 4));
            month = stoi(date_str.substr(5, 2));
            day = stoi(date_str.substr(8, 2));
            return validateDate(year, month, day);
        }
        catch (...) {
            return false;
        }
    }

    // Парсинг US формата (MM/DD/YYYY)
    bool parseUS(const string& date_str, int& year, int& month, int& day) {
        if (date_str.length() < 8) return false;

        size_t slash1 = date_str.find('/');
        size_t slash2 = date_str.find('/', slash1 + 1);
        if (slash1 == string::npos || slash2 == string::npos) return false;

        try {
            month = stoi(date_str.substr(0, slash1));
            day = stoi(date_str.substr(slash1 + 1, slash2 - slash1 - 1));
            year = stoi(date_str.substr(slash2 + 1));

            // Обработка неоднозначности: если день > 12, возможно это EU формат
            if (day > 12 && month <= 12) {
                swap(month, day);
            }

            return validateDate(year, month, day);
        }
        catch (...) {
            return false;
        }
    }

    // Парсинг EU формата (DD.MM.YYYY)
    bool parseEU(const string& date_str, int& year, int& month, int& day) {
        if (date_str.length() < 8) return false;

        size_t dot1 = date_str.find('.');
        size_t dot2 = date_str.find('.', dot1 + 1);
        if (dot1 == string::npos || dot2 == string::npos) return false;

        try {
            day = stoi(date_str.substr(0, dot1));
            month = stoi(date_str.substr(dot1 + 1, dot2 - dot1 - 1));
            year = stoi(date_str.substr(dot2 + 1));

            // Обработка неоднозначности: если месяц > 12, возможно это US формат
            if (month > 12 && day <= 12) {
                swap(month, day);
            }

            return validateDate(year, month, day);
        }
        catch (...) {
            return false;
        }
    }

    // Парсинг текстовой даты
    bool parseText(const string& date_str, int& year, int& month, int& day) {
        string lower = date_str;
        transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        // Ищем месяц в строке
        for (const auto& entry : month_map) {
            const string& month_name = entry.first;
            const string& month_num = entry.second;
            size_t pos = lower.find(month_name);
            if (pos != string::npos) {
                try {
                    // Извлекаем день и год
                    string before = lower.substr(0, pos);
                    string after = lower.substr(pos + month_name.length());

                    // Убираем всё кроме цифр
                    before.erase(remove_if(before.begin(), before.end(),
                        [](char c) { return !isdigit(c); }), before.end());
                    after.erase(remove_if(after.begin(), after.end(),
                        [](char c) { return !isdigit(c); }), after.end());

                    if (!before.empty() && !after.empty()) {
                        int num1 = stoi(before);
                        int num2 = stoi(after);

                        if (num1 <= 31 && num2 >= 1000) {
                            day = num1;
                            year = num2;
                        }
                        else if (num2 <= 31 && num1 >= 1000) {
                            day = num2;
                            year = num1;
                        }
                        else {
                            // Неоднозначность - пробуем оба варианта
                            day = num1;
                            year = num2;
                            if (!validateDate(year, stoi(month_num), day)) {
                                day = num2;
                                year = num1;
                            }
                        }

                        month = stoi(month_num);
                        return validateDate(year, month, day);
                    }
                }
                catch (...) {
                    return false;
                }
            }
        }

        return false;
    }

public:
    // Основная функция конвертации
    DateResult convertDate(const string& raw_date, const string& target_format) {
        DateResult result;
        result.raw_date = raw_date;
        result.valid = false;

        // Проверка пустой/битой даты
        if (raw_date.empty()) {
            result.error_reason = "Пустая строка даты";
            return result;
        }

        if (raw_date.length() > 50) {
            result.error_reason = "Слишком длинная строка";
            return result;
        }

        int year = 0, month = 0, day = 0;
        bool parsed = false;

        // Последовательная проверка всех форматов
        if (parseISO(raw_date, year, month, day)) parsed = true;
        else if (parseUS(raw_date, year, month, day)) parsed = true;
        else if (parseEU(raw_date, year, month, day)) parsed = true;
        else if (parseText(raw_date, year, month, day)) parsed = true;

        if (!parsed) {
            result.error_reason = "Неизвестный формат даты";
            return result;
        }

        // Форматирование ISO даты
        stringstream iso_ss;
        iso_ss << setw(4) << setfill('0') << year << "-"
            << setw(2) << setfill('0') << month << "-"
            << setw(2) << setfill('0') << day;
        result.iso_date = iso_ss.str();

        // Конвертация в целевой формат
        if (target_format == "ru_long") {
            static const vector<string> months_ru = {
                "января", "февраля", "марта", "апреля", "мая", "июня",
                "июля", "августа", "сентября", "октября", "ноября", "декабря"
            };
            result.converted_date = to_string(day) + " " + months_ru[month - 1] + " " + to_string(year);
        }
        else if (target_format == "us") {
            result.converted_date = (month < 10 ? "0" : "") + to_string(month) + "/" +
                (day < 10 ? "0" : "") + to_string(day) + "/" + to_string(year);
        }
        else if (target_format == "eu") {
            result.converted_date = (day < 10 ? "0" : "") + to_string(day) + "." +
                (month < 10 ? "0" : "") + to_string(month) + "." + to_string(year);
        }
        else { // ISO по умолчанию
            result.converted_date = result.iso_date;
        }

        result.valid = true;
        result.error_reason = "";
        return result;
    }

    // Массовая конвертация для бенчмаркинга
    vector<DateResult> convertDatesBatch(const vector<string>& raw_dates,
        const string& target_format,
        double& total_time_ms) {
        vector<DateResult> results;
        results.reserve(raw_dates.size());

        auto start = chrono::high_resolution_clock::now();

        for (const auto& date : raw_dates) {
            results.push_back(convertDate(date, target_format));
        }

        auto end = chrono::high_resolution_clock::now();
        total_time_ms = chrono::duration<double, milli>(end - start).count();

        return results;
    }
};

// ==================== УТИЛИТЫ ДЛЯ РАБОТЫ С JSON ====================

vector<string> loadDatesFromJSON(const string& filename) {
    vector<string> dates;

    try {
        ifstream file(filename);
        if (!file.is_open()) {
            throw runtime_error("Файл не найден: " + filename);
        }

        json data = json::parse(file);

        if (!data.is_array()) {
            throw runtime_error("JSON должен содержать массив объектов");
        }

        for (const auto& item : data) {
            if (item.contains("raw_date") && item["raw_date"].is_string()) {
                dates.push_back(item["raw_date"].get<string>());
            }
        }

    }
    catch (const exception& e) {
        cerr << "Ошибка загрузки JSON: " << e.what() << endl;
    }

    return dates;
}

void generateTestJSON(const string& filename, int count, bool include_errors = true) {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> year_dist(1900, 2100);  // ИСПРАВЛЕНО: явно указан тип
    uniform_int_distribution<int> month_dist(1, 12);      // ИСПРАВЛЕНО: явно указан тип
    uniform_int_distribution<int> day_dist(1, 28);        // ИСПРАВЛЕНО: явно указан тип
    uniform_int_distribution<int> format_dist(0, 6);      // ИСПРАВЛЕНО: явно указан тип

    const vector<string> month_names_ru = {
        "января", "февраля", "марта", "апреля", "мая", "июня",
        "июля", "августа", "сентября", "октября", "ноября", "декабря"
    };

    const vector<string> month_names_en = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };

    json data = json::array();

    for (int i = 0; i < count; i++) {
        json item;
        string raw_date;

        if (include_errors && i % 10 == 0) {
            // 10% ошибок
            switch (i % 5) {
            case 0: raw_date = ""; break;  // Пустая
            case 1: raw_date = "invalid-date"; break;  // Неправильный формат
            case 2: raw_date = "2023-02-30"; break;  // Некорректная дата
            case 3: raw_date = "99/99/9999"; break;  // Невозможная дата
            case 4: raw_date = "сегодня"; break;  // Текстовая ошибка
            default: raw_date = ""; break;
            }
        }
        else {
            int year = year_dist(gen);
            int month = month_dist(gen);
            int day = day_dist(gen);

            // Проверка високосного года
            bool leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
            if (month == 2 && day == 29 && !leap) {
                day = 28;  // Исправляем для невисокосного года
            }

            int format = format_dist(gen);

            switch (format) {
            case 0: // ISO
                raw_date = to_string(year) + "-" +
                    (month < 10 ? "0" : "") + to_string(month) + "-" +
                    (day < 10 ? "0" : "") + to_string(day);
                break;
            case 1: // US формат
                raw_date = to_string(month) + "/" +
                    to_string(day) + "/" + to_string(year);
                break;
            case 2: // EU формат с точкой
                raw_date = to_string(day) + "." +
                    to_string(month) + "." + to_string(year);
                break;
            case 3: // Русский текст
                raw_date = to_string(day) + " " +
                    month_names_ru[month - 1] + " " + to_string(year);
                break;
            case 4: // Английский текст
                raw_date = month_names_en[month - 1] + " " +
                    to_string(day) + ", " + to_string(year);
                break;
            case 5: // Смешанный регистр
                raw_date = "15 МарТа " + to_string(year);
                break;
            case 6: // US с ведущими нулями
                raw_date = (month < 10 ? "0" : "") + to_string(month) + "/" +
                    (day < 10 ? "0" : "") + to_string(day) + "/" + to_string(year);
                break;
            default:
                raw_date = to_string(year) + "-01-01";
                break;
            }
        }

        item["raw_date"] = raw_date;
        data.push_back(item);
    }

    ofstream file(filename);
    file << data.dump(2);
    file.close();

    cout << "Создан файл: " << filename << " (" << count << " записей)" << endl;  // ИСПРАВЛЕНО: убраны спецсимволы
}

void saveResultsToJSON(const vector<DateResult>& results,
    const string& filename,
    const string& target_format) {
    json data = json::array();

    for (const auto& result : results) {
        json item;
        item["raw_date"] = result.raw_date;
        item["valid"] = result.valid;

        if (result.valid) {
            item["iso_date"] = result.iso_date;
            item["converted_date"] = result.converted_date;
            item["target_format"] = target_format;
        }
        else {
            item["error"] = result.error_reason;
        }

        data.push_back(item);
    }

    ofstream file(filename);
    file << data.dump(2);
    file.close();

    cout << "Результаты сохранены в: " << filename << endl;  // ИСПРАВЛЕНО: убраны спецсимволы
}

// ==================== ВИЗУАЛЬНЫЙ ВЫВОД ====================

void printHeader(const string& title) {
    cout << "\n" << string(70, '=') << endl;
    cout << "  " << title << endl;
    cout << string(70, '=') << endl;
}

void printConvertedTable(const vector<DateResult>& results) {
    printHeader("УСПЕШНО СКОНВЕРТИРОВАННЫЕ ДАТЫ");

    cout << left << setw(30) << "Исходная дата"
        << setw(15) << "ISO формат"
        << setw(25) << "Сконвертировано" << endl;
    cout << string(70, '-') << endl;

    int count = 0;
    for (const auto& result : results) {
        if (result.valid) {
            cout << left << setw(30)
                << (result.raw_date.length() > 29 ? result.raw_date.substr(0, 27) + "..." : result.raw_date)
                << setw(15) << result.iso_date
                << setw(25) << result.converted_date << endl;
            count++;
        }
    }

    if (count == 0) {
        cout << "Нет успешно сконвертированных дат" << endl;
    }

    cout << string(70, '-') << endl;
    cout << "Всего успешно: " << count << endl;
}

void printErrorsTable(const vector<DateResult>& results) {
    printHeader("ОШИБКИ КОНВЕРТАЦИИ");

    cout << left << setw(30) << "Исходная дата"
        << setw(40) << "Причина ошибки" << endl;
    cout << string(70, '-') << endl;

    int error_count = 0;
    for (const auto& result : results) {
        if (!result.valid) {
            cout << left << setw(30)
                << (result.raw_date.length() > 29 ? result.raw_date.substr(0, 27) + "..." : result.raw_date)
                << setw(40) << result.error_reason << endl;
            error_count++;
        }
    }

    if (error_count == 0) {
        cout << "Нет ошибок конвертации" << endl;
    }

    cout << string(70, '-') << endl;
    cout << "Всего с ошибками: " << error_count << endl;
}

void printStatistics(const vector<DateResult>& results, double time_ms = 0.0) {
    printHeader("СТАТИСТИКА КОНВЕРТАЦИИ");

    int total = (int)results.size();  // ИСПРАВЛЕНО: приведение типа
    int valid = 0;
    int errors = 0;

    for (const auto& result : results) {
        if (result.valid) valid++;
        else errors++;
    }

    // ИСПРАВЛЕНО: убраны манипуляторы из cout и использован правильный синтаксис
    cout << "Всего дат:        " << total << endl;
    cout << "Успешно:          " << valid << " (";
    if (total > 0) {
        cout << fixed << setprecision(1) << (valid * 100.0 / total) << "%)" << endl;
    }
    else {
        cout << "0.0%)" << endl;
    }
    cout << "С ошибками:       " << errors << " (";
    if (total > 0) {
        cout << fixed << setprecision(1) << (errors * 100.0 / total) << "%)" << endl;
    }
    else {
        cout << "0.0%)" << endl;
    }

    if (time_ms > 0) {
        cout << "\nПроизводительность:" << endl;
        cout << "Время выполнения: " << fixed << setprecision(2) << time_ms << " мс" << endl;
        cout << "Скорость:         " << fixed << setprecision(0)
            << (total / (time_ms / 1000.0)) << " дат/сек" << endl;
    }
}

void runBenchmark() {
    printHeader("БЕНЧМАРК ПРОИЗВОДИТЕЛЬНОСТИ");

    cout << "Тестирование массовой конверсии (узкое место - много ветвлений парсинга)..." << endl << endl;

    vector<int> test_sizes = { 100, 1000, 10000, 50000 };

    cout << left << setw(12) << "Кол-во дат"
        << setw(20) << "Время конвертации"
        << setw(15) << "Скорость" << endl;
    cout << string(47, '-') << endl;

    for (int size : test_sizes) {
        // Генерируем тестовые данные
        string filename = "benchmark_" + to_string(size) + ".json";
        generateTestJSON(filename, size, false);

        // Загружаем даты
        auto dates = loadDatesFromJSON(filename);

        // Замер времени
        DateConverter converter;
        double total_time = 0.0;

        auto results = converter.convertDatesBatch(dates, "ru_long", total_time);

        double speed = (total_time > 0) ? (size / (total_time / 1000.0)) : 0;

        cout << left << setw(12) << size
            << setw(20) << fixed << setprecision(1) << total_time << " мс"
            << setw(15) << fixed << setprecision(0) << speed << " дат/сек" << endl;

        // Удаляем временный файл
        remove(filename.c_str());
    }

    // Анализ узкого места
    printHeader("АНАЛИЗ УЗКОГО МЕСТА");

    cout << "УЗКОЕ МЕСТО: много ветвлений в функции convertDate()" << endl << endl;
    cout << "Причины замедления:" << endl;
    cout << "1. Последовательная проверка 4-х форматов дат" << endl;
    cout << "   - parseISO() -> parseUS() -> parseEU() -> parseText()" << endl;
    cout << "2. Преобразование в нижний регистр для каждого текстового поиска" << endl;
    cout << "3. Многократный поиск месяца в строке" << endl;
    cout << "4. Использование std::remove_if для очистки строк" << endl << endl;

    cout << "Предложения по оптимизации:" << endl;
    cout << "1. Определять формат по первым символам" << endl;
    cout << "2. Предварительно компилировать regex паттерны" << endl;
    cout << "3. Кэшировать преобразованные в нижний регистр строки" << endl;
    cout << "4. Использовать хэш-таблицы для быстрого поиска месяцев" << endl;
    cout << "5. Применить многопоточность для больших объемов" << endl;
}

void showHelp() {
    printHeader("КОНВЕРТЕР ДАТ ИЗ JSON - СПРАВКА");

    cout << "\nИспользование:" << endl;
    cout << "  --input <файл>     Входной JSON файл с датами" << endl;
    cout << "  --to <формат>      Целевой формат: iso, ru_long, us, eu" << endl;
    cout << "  --output <файл>    Выходной JSON файл (опционально)" << endl;
    cout << "  --benchmark        Запустить бенчмарк производительности" << endl;
    cout << "  --generate <кол-во> Сгенерировать тестовые данные" << endl;
    cout << "  --help             Показать эту справку" << endl << endl;

    cout << "Примеры:" << endl;
    cout << "  date_converter --input dates.json --to ru_long" << endl;
    cout << "  date_converter --generate 1000" << endl;
    cout << "  date_converter --benchmark" << endl << endl;

    cout << "Форматы дат на входе:" << endl;
    cout << "  ISO: 2025-11-30" << endl;
    cout << "  US: 11/30/2025 или 01/15/2025" << endl;
    cout << "  EU: 30.11.2025 или 15.01.2025" << endl;
    cout << "  Русский: 30 ноября 2025 или 15 января 2025" << endl;
    cout << "  Английский: November 30, 2025 или Jan 15, 2024" << endl << endl;

    cout << "Обработка ошибок:" << endl;
    cout << "  Пустые даты" << endl;
    cout << "  Неправильные форматы" << endl;
    cout << "  Некорректные даты (2023-02-29)" << endl;
    cout << "  Выход за границы (год 1900-2100)" << endl;
}

// ==================== ГЛАВНАЯ ФУНКЦИЯ ====================

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "rus");

    cout << "========================================================" << endl;
    cout << " КОНВЕРТЕР ДАТ ИЗ JSON ФАЙЛОВ" << endl;
    cout << " Форматы: ISO, US, EU, русский, английский" << endl;
    cout << "========================================================" << endl;

    // Обработка аргументов командной строки
    if (argc > 1) {
        vector<string> args(argv + 1, argv + argc);

        for (size_t i = 0; i < args.size(); i++) {
            if (args[i] == "--help" || args[i] == "-h") {
                showHelp();
                return 0;
            }
            else if (args[i] == "--benchmark" || args[i] == "-b") {
                runBenchmark();
                return 0;
            }
            else if (args[i] == "--generate" || args[i] == "-g") {
                if (i + 1 < args.size()) {
                    int count = stoi(args[i + 1]);
                    generateTestJSON("dates_" + to_string(count) + ".json", count, true);
                }
                else {
                    cout << "Укажите количество дат: --generate 1000" << endl;
                }
                return 0;
            }
            else if (args[i] == "--input" || args[i] == "-i") {
                if (i + 2 < args.size()) {
                    string input_file = args[i + 1];
                    string target_format = args[i + 2];
                    string output_file = (i + 3 < args.size()) ? args[i + 3] : "";

                    auto dates = loadDatesFromJSON(input_file);
                    if (dates.empty()) {
                        cout << "Нет данных для обработки" << endl;
                        return 1;
                    }

                    DateConverter converter;
                    double total_time = 0.0;
                    auto results = converter.convertDatesBatch(dates, target_format, total_time);

                    printConvertedTable(results);
                    printErrorsTable(results);
                    printStatistics(results, total_time);

                    if (!output_file.empty()) {
                        saveResultsToJSON(results, output_file, target_format);
                    }
                }
                else {
                    cout << "Использование: --input файл.json формат [выходной_файл.json]" << endl;
                }
                return 0;
            }
        }
    }

    // Интерактивный режим
    while (true) {
        cout << "\nМЕНЮ:" << endl;
        cout << "1) Конвертировать JSON файл" << endl;
        cout << "2) Запустить бенчмарк производительности" << endl;
        cout << "3) Сгенерировать тестовые данные" << endl;
        cout << "4) Справка" << endl;
        cout << "5) Выход" << endl;
        cout << "Выберите действие (1-5): ";

        string choice;
        getline(cin, choice);

        if (choice == "1") {
            string input_file, target_format, output_file;

            cout << "\nВведите имя входного JSON файла: ";
            getline(cin >> ws, input_file);

            cout << "Целевой формат (iso/ru_long/us/eu) [iso]: ";
            getline(cin, target_format);
            if (target_format.empty()) target_format = "iso";

            cout << "Сохранить результат? (имя файла или ENTER=нет): ";
            getline(cin, output_file);

            try {
                // Загрузка данных
                auto dates = loadDatesFromJSON(input_file);

                if (dates.empty()) {
                    cout << "Нет данных для обработки" << endl;
                    continue;
                }

                cout << "Найдено " << dates.size() << " дат" << endl;
                cout << "Конвертация в формат: " << target_format << "..." << endl;

                // Конвертация с замером времени
                DateConverter converter;
                double total_time = 0.0;
                auto results = converter.convertDatesBatch(dates, target_format, total_time);

                // Вывод результатов
                printConvertedTable(results);
                printErrorsTable(results);
                printStatistics(results, total_time);

                // Сохранение результатов
                if (!output_file.empty()) {
                    saveResultsToJSON(results, output_file, target_format);
                }

            }
            catch (const exception& e) {
                cout << "\nОШИБКА: " << e.what() << endl;
            }
        }
        else if (choice == "2") {
            runBenchmark();
        }
        else if (choice == "3") {
            cout << "Сколько дат сгенерировать? (1-100000): ";
            int count;
            cin >> count;
            cin.ignore();

            if (count > 0 && count <= 100000) {
                cout << "Имя файла [dates_" << count << ".json]: ";
                string filename;
                getline(cin, filename);
                if (filename.empty()) {
                    filename = "dates_" + to_string(count) + ".json";
                }
                generateTestJSON(filename, count, true);
            }
            else {
                cout << "Некорректное количество" << endl;
            }
        }
        else if (choice == "4") {
            showHelp();
        }
        else if (choice == "5") {
            cout << "\nДо свидания!" << endl;
            break;
        }
        else {
            cout << "Выберите действие 1-5" << endl;
        }
    }

    return 0;
}