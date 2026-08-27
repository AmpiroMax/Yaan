/*
Created: 27:08:2026 - 23:05:00
Last updated: 27:08:2026 - 23:05:00
Module: tools
File: tools/known_findings.h

Responsibility:
- СПИСОК ИЗВЕСТНЫХ НАХОДОК для судей (dfn_stairs_check, dfn_interior_check).
  Прибор, заведённый в ctest на дереве, которое ЕЩЁ чинят, обязан различать
  три вещи, а не две: «чисто», «известная беда, у неё есть дата и причина, её
  жжёт названная волна» и «НОВАЯ беда». Без среднего состояния прибор либо
  выключают (и он молчит месяц), либо он краснеет всегда (и его перестают
  читать) — оба исхода уже случались в этом репозитории.

- КЛЮЧ — (ТЕЛО, КЛАСС, СКОЛЬКО), А НЕ ТЕКСТ НАХОДКИ. Текст несёт замеры, а
  замеры при живой правке геометрии плывут каждый час: список по тексту
  протух бы в тот же день и объявил бы КАЖДУЮ сдвинутую бочку новой находкой.
  Класс не плывёт. Число — тот самый предохранитель, ради которого весь
  механизм: 50 «предмет выше пола» в постоялом дворе известны, пятьдесят
  первый — красный сразу.

Usage:
    KnownFindings known;
    std::string err;
    known.load("tests/known_stair_findings.txt", err);
    if (known.take(body, "ПРОСТУПЬ")) { ...печатать как известную... }
    known.report_stale(stdout);

  Формат файла (UTF-8, строки с # — комментарий):
      <тело> | <КЛАСС> | <сколько> | <дата> | <причина, кто жжёт>
  «тело» — имя .dfh или путь .scene; сверка идёт и по полной строке, и по
  ИМЕНИ файла, чтобы список не зависел от того, как судью позвали.

Dependencies:
- Uses: только стандартная библиотека.
- Used by: tools/check_stairs.cpp, tools/check_interior.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- СПИСОК НЕ ЛЕЧИТ, А ОТСРАЧИВАЕТ. Каждая строка обязана нести дату и причину;
  строка без них — это молча выключенный прибор, ровно то, от чего механизм
  и заведён.
- ПРОТУХШАЯ СТРОКА (находки больше нет) НЕ КРАСИТ ТЕСТ, и это решение, а не
  недосмотр: список — ОБЩИЙ файл с волной, которая чинит геометрию, и
  красный на её правильной починке остановил бы её же в те секунды, пока она
  не дошла до списка (правило 29 про общий файл). Она печатается громко и
  жжётся отдельной правкой.
*/
/*
UPD:
- 27:08:2026 - 23:05:00: Создан. Волна приборов 27.08 (аудит «Большой мир»,
  задачи 1 и 5): оба судьи заводятся в ctest по ВСЕЙ полке и ВСЕМ 130
  локациям, а геометрию чинит параллельная волна — нужен третий ответ между
  «чисто» и «красный».
*/
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace dfn::tools {

/// Имя файла из пути — без каталогов. Сверка идёт по обоим написаниям.
[[nodiscard]] inline std::string base_name(const std::string& path) {
    const std::size_t p = path.find_last_of("/\\");
    return p == std::string::npos ? path : path.substr(p + 1);
}

[[nodiscard]] inline std::string trimmed(const std::string& s) {
    const std::size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) {
        return {};
    }
    const std::size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

class KnownFindings {
  public:
    struct Entry {
        std::string body;
        std::string cls;
        int budget = 0;
        int used = 0;
        std::string date;
        std::string reason;
        int line = 0;
    };

    /// Прочитать список. Пустой путь — список не заведён (всё красное).
    /// false и текст в err — файл назвали, но он не читается или кривой:
    /// это отказ прибора, а не «списка нет».
    bool load(const std::string& path, std::string& err) {
        path_ = path;
        if (path.empty()) {
            return true;
        }
        std::ifstream in(path);
        if (!in.good()) {
            err = "список известных находок не открылся: " + path;
            return false;
        }
        std::string raw;
        int line_no = 0;
        while (std::getline(in, raw)) {
            ++line_no;
            const std::string line = trimmed(raw);
            if (line.empty() || line[0] == '#') {
                continue;
            }
            std::vector<std::string> f;
            std::size_t from = 0;
            while (true) {
                const std::size_t bar = line.find('|', from);
                f.push_back(trimmed(line.substr(
                    from, bar == std::string::npos ? std::string::npos : bar - from)));
                if (bar == std::string::npos) {
                    break;
                }
                from = bar + 1;
            }
            if (f.size() < 5) {
                err = path + ":" + std::to_string(line_no)
                    + ": нужно пять полей «тело | КЛАСС | сколько | дата | причина»";
                return false;
            }
            Entry e;
            e.body = f[0];
            e.cls = f[1];
            e.budget = std::atoi(f[2].c_str());
            e.date = f[3];
            e.reason = f[4];
            e.line = line_no;
            if (e.body.empty() || e.cls.empty() || e.budget <= 0 || e.date.empty()
                || e.reason.empty()) {
                err = path + ":" + std::to_string(line_no)
                    + ": пустое поле или «сколько» не положительно — строка без "
                      "даты и причины это молча выключенный прибор";
                return false;
            }
            entries_.push_back(std::move(e));
        }
        return true;
    }

    [[nodiscard]] bool empty() const { return entries_.empty(); }
    [[nodiscard]] const std::string& path() const { return path_; }

    /// Списать одну находку в счёт известных. true — известная.
    /// Причина возвращается в reason_out, чтобы судья печатал её рядом.
    bool take(const std::string& body, const std::string& cls,
              std::string* reason_out = nullptr) {
        const std::string base = base_name(body);
        for (Entry& e : entries_) {
            if (e.cls != cls) {
                continue;
            }
            if (e.body != body && e.body != base && base_name(e.body) != base) {
                continue;
            }
            if (e.used >= e.budget) {
                continue; // бюджет исчерпан — дальше это НОВАЯ находка
            }
            ++e.used;
            if (reason_out != nullptr) {
                *reason_out = e.date + ": " + e.reason;
            }
            return true;
        }
        return false;
    }

    /// Что в списке не пригодилось. НЕ красит тест (см. шапку), но кричит.
    /// Возвращает число протухших строк.
    int report_stale(std::FILE* out) const {
        int stale = 0;
        for (const Entry& e : entries_) {
            if (e.used < e.budget) {
                ++stale;
                std::fprintf(out,
                             "СПИСОК ПРОТУХ: %s:%d — «%s | %s» ждал %d, пришло %d. "
                             "Находка сгорела, убери строку.\n",
                             path_.c_str(), e.line, e.body.c_str(), e.cls.c_str(),
                             e.budget, e.used);
            }
        }
        return stale;
    }

    /// Сколько находок списано на известные — для итоговой строки судьи.
    [[nodiscard]] int taken() const {
        int n = 0;
        for (const Entry& e : entries_) {
            n += e.used;
        }
        return n;
    }

  private:
    std::string path_;
    std::vector<Entry> entries_;
};

/// ОЖИДАНИЕ ПО КЛАССАМ — оснастка контрольной руки (правило 30).
/// «--expect N» требует РОВНО N боевых находок всего;
/// «--expect КЛАСС:N» — ровно N находок этого класса. Второе куплено тем, что
/// у настоящего отвергнутого случая находка почти никогда не одна: у стремянки
/// постоялого двора красная не только проступь, и «всего 3» не доказывает, что
/// покраснела ИМЕННО испытуемая рука.
class Expectations {
  public:
    void add(const std::string& arg) {
        const std::size_t colon = arg.find(':');
        if (colon == std::string::npos) {
            total_ = std::atol(arg.c_str());
            has_total_ = true;
            return;
        }
        by_class_.emplace_back(arg.substr(0, colon),
                               std::atol(arg.c_str() + colon + 1));
    }

    [[nodiscard]] bool any() const { return has_total_ || !by_class_.empty(); }

    /// 0 — сошлось; иначе печатает расхождение и возвращает 1.
    [[nodiscard]] int verdict(long got_total,
                              const std::map<std::string, long>& got_by_class,
                              std::FILE* out) const {
        int rc = 0;
        if (has_total_ && got_total != total_) {
            std::fprintf(out,
                         "ОЖИДАЛОСЬ РОВНО %ld находок, получено %ld — прибор "
                         "меряет не то\n",
                         total_, got_total);
            rc = 1;
        }
        for (const auto& [cls, want] : by_class_) {
            const auto it = got_by_class.find(cls);
            const long got = it == got_by_class.end() ? 0 : it->second;
            if (got != want) {
                std::fprintf(out,
                             "ОЖИДАЛОСЬ РОВНО %ld находок класса «%s», получено "
                             "%ld — контрольная рука не сработала\n",
                             want, cls.c_str(), got);
                rc = 1;
            }
        }
        if (rc == 0) {
            std::fprintf(stdout, "оснастка сошлась\n");
        }
        return rc;
    }

  private:
    bool has_total_ = false;
    long total_ = 0;
    std::vector<std::pair<std::string, long>> by_class_;
};

} // namespace dfn::tools
