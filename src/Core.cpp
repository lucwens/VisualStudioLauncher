#include "Core.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace launcher
{
namespace
{
struct Json
{
    enum class Kind
    {
        Null,
        String,
        Number,
        Boolean,
        Object,
        Array
    };
    Kind kind = Kind::Null;
    std::string text;
    bool boolean = false;
    std::map<std::string, Json> object;
    std::vector<Json> array;
};

class Parser
{
  public:
    explicit Parser(const std::string &source) : source_(source)
    {
    }

    Json Parse()
    {
        auto value = Value(0);
        Space();
        if (position_ != source_.size())
        {
            Fail("Unexpected trailing data");
        }
        return value;
    }

  private:
    const std::string &source_;
    size_t position_ = 0;

    [[noreturn]] void Fail(const char *message) const
    {
        throw std::runtime_error(std::string(message) + " at byte " + std::to_string(position_));
    }

    void Space()
    {
        while (position_ < source_.size() && (source_[position_] == ' ' || source_[position_] == '\t' ||
                                              source_[position_] == '\r' || source_[position_] == '\n'))
        {
            ++position_;
        }
    }

    bool Take(char c)
    {
        Space();
        if (position_ < source_.size() && source_[position_] == c)
        {
            ++position_;
            return true;
        }
        return false;
    }

    unsigned Hex4()
    {
        unsigned result = 0;
        for (int i = 0; i < 4; ++i)
        {
            if (position_ == source_.size())
            {
                Fail("Incomplete Unicode escape");
            }
            const auto c = static_cast<unsigned char>(source_[position_++]);
            unsigned digit = 0;
            if (c >= '0' && c <= '9')
            {
                digit = c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                digit = c - 'a' + 10;
            }
            else if (c >= 'A' && c <= 'F')
            {
                digit = c - 'A' + 10;
            }
            else
            {
                Fail("Invalid Unicode escape");
            }
            result = result * 16 + digit;
        }
        return result;
    }

    static void Utf8(std::string &output, unsigned cp)
    {
        if (cp < 0x80)
        {
            output += static_cast<char>(cp);
        }
        else if (cp < 0x800)
        {
            output += static_cast<char>(0xC0 | (cp >> 6));
            output += static_cast<char>(0x80 | (cp & 63));
        }
        else if (cp < 0x10000)
        {
            output += static_cast<char>(0xE0 | (cp >> 12));
            output += static_cast<char>(0x80 | ((cp >> 6) & 63));
            output += static_cast<char>(0x80 | (cp & 63));
        }
        else
        {
            output += static_cast<char>(0xF0 | (cp >> 18));
            output += static_cast<char>(0x80 | ((cp >> 12) & 63));
            output += static_cast<char>(0x80 | ((cp >> 6) & 63));
            output += static_cast<char>(0x80 | (cp & 63));
        }
    }

    std::string String()
    {
        if (!Take('"'))
        {
            Fail("Expected string");
        }
        std::string result;
        while (position_ < source_.size())
        {
            auto c = static_cast<unsigned char>(source_[position_++]);
            if (c == '"')
            {
                return result;
            }
            if (c < 32)
            {
                Fail("Control character in string");
            }
            if (c != '\\')
            {
                result += static_cast<char>(c);
                continue;
            }
            if (position_ == source_.size())
            {
                Fail("Incomplete escape");
            }
            c = static_cast<unsigned char>(source_[position_++]);
            switch (c)
            {
            case '"':
                result += '"';
                break;
            case '\\':
                result += '\\';
                break;
            case '/':
                result += '/';
                break;
            case 'b':
                result += '\b';
                break;
            case 'f':
                result += '\f';
                break;
            case 'n':
                result += '\n';
                break;
            case 'r':
                result += '\r';
                break;
            case 't':
                result += '\t';
                break;
            case 'u':
            {
                unsigned cp = Hex4();
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    if (position_ + 2 > source_.size() || source_[position_] != '\\' || source_[position_ + 1] != 'u')
                    {
                        Fail("Missing low surrogate");
                    }
                    position_ += 2;
                    const unsigned low = Hex4();
                    if (low < 0xDC00 || low > 0xDFFF)
                    {
                        Fail("Invalid low surrogate");
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + low - 0xDC00;
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                {
                    Fail("Unexpected low surrogate");
                }
                Utf8(result, cp);
                break;
            }
            default:
                Fail("Invalid escape");
            }
        }
        Fail("Unterminated string");
    }

    Json Value(int depth)
    {
        if (depth > 32)
        {
            Fail("JSON nesting limit exceeded");
        }
        Space();
        if (position_ == source_.size())
        {
            Fail("Expected value");
        }
        Json value;
        const char c = source_[position_];
        if (c == '"')
        {
            value.kind = Json::Kind::String;
            value.text = String();
        }
        else if (Take('{'))
        {
            value.kind = Json::Kind::Object;
            if (Take('}'))
            {
                return value;
            }
            do
            {
                const auto key = String();
                if (!Take(':'))
                {
                    Fail("Expected colon");
                }
                if (!value.object.emplace(key, Value(depth + 1)).second)
                {
                    Fail("Duplicate object key");
                }
            } while (Take(','));
            if (!Take('}'))
            {
                Fail("Expected closing brace");
            }
        }
        else if (Take('['))
        {
            value.kind = Json::Kind::Array;
            if (Take(']'))
            {
                return value;
            }
            do
            {
                value.array.push_back(Value(depth + 1));
            } while (Take(','));
            if (!Take(']'))
            {
                Fail("Expected closing bracket");
            }
        }
        else if (source_.compare(position_, 4, "true") == 0)
        {
            position_ += 4;
            value.kind = Json::Kind::Boolean;
            value.boolean = true;
        }
        else if (source_.compare(position_, 5, "false") == 0)
        {
            position_ += 5;
            value.kind = Json::Kind::Boolean;
        }
        else if (source_.compare(position_, 4, "null") == 0)
        {
            position_ += 4;
        }
        else if (c == '-' || (c >= '0' && c <= '9'))
        {
            value.kind = Json::Kind::Number;
            const auto start = position_;
            if (source_[position_] == '-')
            {
                ++position_;
            }
            if (position_ == source_.size())
            {
                Fail("Invalid number");
            }
            if (source_[position_] == '0')
            {
                ++position_;
            }
            else
            {
                if (source_[position_] < '1' || source_[position_] > '9')
                {
                    Fail("Invalid number");
                }
                while (position_ < source_.size() && std::isdigit(static_cast<unsigned char>(source_[position_])))
                {
                    ++position_;
                }
            }
            // The settings schema contains integer version numbers only.
            value.text = source_.substr(start, position_ - start);
        }
        else
        {
            Fail("Unexpected value");
        }
        return value;
    }
};

const Json &Member(const Json &object, const char *name)
{
    const auto it = object.object.find(name);
    if (object.kind != Json::Kind::Object || it == object.object.end())
    {
        throw std::runtime_error(std::string("Missing settings field: ") + name);
    }
    return it->second;
}

std::string Text(const Json &object, const char *name, const std::string &fallback = "")
{
    const auto it = object.object.find(name);
    if (it == object.object.end())
    {
        return fallback;
    }
    if (it->second.kind != Json::Kind::String)
    {
        throw std::runtime_error(std::string("Expected a string for ") + name);
    }
    return it->second.text;
}

std::string Escape(const std::string &text)
{
    constexpr char hex[] = "0123456789abcdef";
    std::string output = "\"";
    for (const unsigned char c : text)
    {
        if (c == '"' || c == '\\')
        {
            output += '\\';
            output += static_cast<char>(c);
        }
        else if (c < 32)
        {
            output += "\\u00";
            output += hex[c >> 4];
            output += hex[c & 15];
        }
        else
        {
            output += static_cast<char>(c);
        }
    }
    return output + '"';
}
} // namespace

bool IsValidId(const std::string &id)
{
    return !id.empty() && id.size() <= 80 &&
           std::all_of(
               id.begin(), id.end(), [](unsigned char c)
               { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-'; });
}

bool IsValidColor(const std::string &color)
{
    return color.size() == 7 && color[0] == '#' &&
           std::all_of(color.begin() + 1, color.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
}

bool IsSolutionPath(const std::string &path)
{
    const auto pos = path.find_last_of('.');
    if (pos == std::string::npos)
    {
        return false;
    }
    auto ext = path.substr(pos);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".sln" || ext == ".slnx";
}

Settings ParseSettings(const std::string &json)
{
    const auto root = Parser(json).Parse();
    const auto &version = Member(root, "version");
    if (version.kind != Json::Kind::Number || version.text != "1")
    {
        throw std::runtime_error("Unsupported settings version; the file was not changed.");
    }
    Settings settings;
    settings.visualStudio = Text(root, "visualStudio");
    const auto &entries = Member(root, "entries");
    if (entries.kind != Json::Kind::Array)
    {
        throw std::runtime_error("entries must be an array");
    }
    if (entries.array.size() > 500)
    {
        throw std::runtime_error("Maximum 500 configured solutions");
    }
    std::set<std::string> ids;
    for (const auto &value : entries.array)
    {
        if (value.kind != Json::Kind::Object)
        {
            throw std::runtime_error("Each entry must be an object");
        }
        Entry entry;
        entry.id = Text(value, "id");
        if (!IsValidId(entry.id) || !ids.insert(entry.id).second)
        {
            throw std::runtime_error("Invalid or duplicate entry ID");
        }
        entry.name = Text(value, "name");
        entry.group = Text(value, "group", "Solutions");
        entry.branch = Text(value, "branch");
        const auto fromGit = value.object.find("branchFromGit");
        if (fromGit != value.object.end())
        {
            if (fromGit->second.kind != Json::Kind::Boolean)
            {
                throw std::runtime_error("branchFromGit must be boolean");
            }
            entry.branchFromGit = fromGit->second.boolean;
        }
        entry.path = Text(value, "path");
        entry.color = Text(value, "color", "#2563EB");
        if (!IsValidColor(entry.color))
        {
            throw std::runtime_error("Invalid icon color");
        }
        entry.icon = Text(value, "icon");
        entry.visualStudio = Text(value, "visualStudio");
        const auto enabled = value.object.find("enabled");
        if (enabled != value.object.end())
        {
            if (enabled->second.kind != Json::Kind::Boolean)
            {
                throw std::runtime_error("enabled must be boolean");
            }
            entry.enabled = enabled->second.boolean;
        }
        settings.entries.push_back(std::move(entry));
    }
    return settings;
}

std::vector<RecentSolution> ParseRecentSolutions(const std::string &json)
{
    const auto root = Parser(json).Parse();
    if (root.kind != Json::Kind::Array)
    {
        throw std::runtime_error("Recent list must be an array");
    }
    std::vector<RecentSolution> result;
    for (const auto &item : root.array)
    {
        if (item.kind != Json::Kind::Object)
        {
            continue;
        }
        const auto value = item.object.find("Value");
        if (value == item.object.end() || value->second.kind != Json::Kind::Object)
        {
            continue;
        }
        const auto local = value->second.object.find("LocalProperties");
        if (local == value->second.object.end() || local->second.kind != Json::Kind::Object)
        {
            continue;
        }
        const auto path = local->second.object.find("FullPath");
        const auto type = local->second.object.find("Type");
        if (path == local->second.object.end() || path->second.kind != Json::Kind::String ||
            (type != local->second.object.end() && type->second.text != "0") || !IsSolutionPath(path->second.text))
        {
            continue; // Folders and loose projects are not launchable solutions.
        }
        RecentSolution recent;
        recent.path = path->second.text;
        const auto accessed = value->second.object.find("LastAccessed");
        if (accessed != value->second.object.end() && accessed->second.kind == Json::Kind::String)
        {
            recent.lastAccessed = accessed->second.text;
        }
        result.push_back(std::move(recent));
    }
    std::stable_sort(result.begin(), result.end(), [](const RecentSolution &a, const RecentSolution &b)
                     { return a.lastAccessed > b.lastAccessed; });
    return result;
}

std::string SerializeSettings(const Settings &settings)
{
    std::ostringstream out;
    out << "{\n  \"version\": 1,\n  \"visualStudio\": " << Escape(settings.visualStudio) << ",\n  \"entries\": [\n";
    for (size_t i = 0; i < settings.entries.size(); ++i)
    {
        const auto &e = settings.entries[i];
        out << "    {\n      \"id\": " << Escape(e.id) << ",\n      \"name\": " << Escape(e.name)
            << ",\n      \"group\": " << Escape(e.group) << ",\n      \"branch\": " << Escape(e.branch)
            << ",\n      \"branchFromGit\": " << (e.branchFromGit ? "true" : "false")
            << ",\n      \"path\": " << Escape(e.path) << ",\n      \"color\": " << Escape(e.color)
            << ",\n      \"icon\": " << Escape(e.icon) << ",\n      \"visualStudio\": " << Escape(e.visualStudio)
            << ",\n      \"enabled\": " << (e.enabled ? "true" : "false") << "\n    }"
            << (i + 1 == settings.entries.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
    return out.str();
}

std::string DisplayName(const Entry &entry)
{
    return DisplayName(entry.name, entry.branch);
}

std::string DisplayName(const std::string &name, const std::string &branch)
{
    return name + (branch.empty() ? "" : " [" + branch + "]");
}

std::string BranchFromHead(const std::string &head)
{
    const auto end = head.find_last_not_of(" \t\r\n");
    const auto line = head.substr(0, end == std::string::npos ? 0 : end + 1);
    const std::string prefix = "ref: refs/heads/";
    if (line.compare(0, prefix.size(), prefix) == 0)
    {
        return line.substr(prefix.size());
    }
    if (line.size() >= 40 &&
        std::all_of(line.begin(), line.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
    {
        return line.substr(0, 7); // Detached HEAD: show the short commit hash.
    }
    return {};
}

std::wstring QuoteArgument(const std::wstring &argument)
{
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (const wchar_t c : argument)
    {
        if (c == L'\\')
        {
            ++slashes;
            continue;
        }
        if (c == L'"')
        {
            result.append(slashes * 2 + 1, L'\\');
        }
        else
        {
            result.append(slashes, L'\\');
        }
        slashes = 0;
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
} // namespace launcher
