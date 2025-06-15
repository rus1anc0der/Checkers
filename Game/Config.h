#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

/**
 * Класс для работы с конфигурационными настройками приложения.
 * Обеспечивает загрузку параметров из JSON-файла и удобный доступ к ним.
 */
class Config
{
public:
    Config()
    {
        reload(); // Автоматическая загрузка настроек при создании объекта
    }

    /**
     * Загружает или перезагружает конфигурационные данные из файла.
     * 
     * Алгоритм работы:
     * 1. Открывает файл настроек "settings.json" в корневой директории проекта
     * 2. Парсит содержимое файла в JSON-объект
     * 3. Закрывает файловый поток
     */
    void reload()
    {
        std::ifstream fin(project_path + "settings.json");
        fin >> config; // Десериализация JSON в объект config
        fin.close();
    }

    /**
     * Оператор для удобного доступа к настройкам.
     * 
     * @param section Название раздела конфигурации (например, "Bot")
     * @param param_name Имя параметра в разделе (например, "IsWhiteBot")
     * @return Значение запрашиваемого параметра
     * 
     * Пример использования:
     * bool is_bot = config("Bot", "IsWhiteBot");
     */
    auto operator()(const string& setting_dir, const string& setting_name) const
    {
        return config[setting_dir][setting_name];
    }

private:
    json config; // Хранилище конфигурационных данных в формате JSON
};
