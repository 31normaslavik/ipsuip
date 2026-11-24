#pragma once

/** Преобразования для сохранения IP-списков: диапазоны -> CIDR, имена файлов. **/

#include <string>
#include <vector>

namespace ipsuip {

// "a.b.c.d-a.b.c.d" -> минимальный набор CIDR-блоков.
// Возвращает пустой вектор, если строка не является IPv4-диапазоном.
std::vector<std::string> range_to_cidr(const std::string &range);

// Преобразует список диапазонов в CIDR; нераспознанные строки (например,
// IPv6) остаются без изменений.
std::vector<std::string> ranges_to_cidr(const std::vector<std::string> &ranges);

// Приводит произвольное название к безопасному имени файла:
// недопустимые символы заменяются на '_', повторы '_' схлопываются,
// ведущие и хвостовые '_' обрезаются.
std::string sanitize_filename(const std::string &name);

} // namespace ipsuip
