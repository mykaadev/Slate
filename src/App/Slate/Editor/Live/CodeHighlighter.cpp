#include "App/Slate/Editor/Live/CodeHighlighter.h"

#include <algorithm>
#include <cctype>
#include <vector>
#include <string>
#include <unordered_set>

namespace Software::Slate::Live
{
    namespace
    {
        // State values carried between lines
        constexpr int StateBlockComment = 1;
        constexpr int StateTripleDouble = 2;
        constexpr int StateTripleSingle = 3;
        constexpr int StateBacktick = 4;
        constexpr int StateLuaLongString = 5;
        constexpr int StateLuaLongComment = 6;

        struct LanguageSpec
        {
            std::vector<const char*> names;
            std::unordered_set<std::string> keywords;
            std::unordered_set<std::string> types;
            const char* lineComment = nullptr;
            const char* lineComment2 = nullptr;
            const char* blockOpen = nullptr;
            const char* blockClose = nullptr;
            bool singleQuoteStrings = true;
            bool backtickStrings = false;
            bool tripleQuotes = false;
            bool hashMeta = false;       // #include / #define lines
            bool atMeta = false;         // @decorators and annotations
            bool dollarVariables = false;
            bool caseInsensitive = false;
            bool capitalizedTypes = true;
            bool luaStrings = false;
            bool markup = false;         // <tags>
            bool keyValue = false;       // key: value / key = value
        };

        const std::vector<LanguageSpec>& Specs()
        {
            static const std::vector<LanguageSpec> specs = [] {
                std::vector<LanguageSpec> list;

                LanguageSpec c;
                c.names = {"c", "h", "cpp", "c++", "cc", "cxx", "hpp", "hlsl", "glsl", "shader", "objc"};
                c.keywords = {"auto", "break", "case", "catch", "class", "const", "constexpr", "continue", "default",
                              "delete", "do", "else", "enum", "explicit", "export", "extern", "final", "for", "friend",
                              "goto", "if", "inline", "mutable", "namespace", "new", "noexcept", "operator", "override",
                              "private", "protected", "public", "return", "sizeof", "static", "static_cast", "struct",
                              "switch", "template", "this", "throw", "try", "typedef", "typename", "union", "using",
                              "virtual", "volatile", "while", "co_await", "co_return", "co_yield", "concept", "requires",
                              "reinterpret_cast", "dynamic_cast", "const_cast", "uniform", "in", "out", "inout"};
                c.types = {"void", "bool", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
                           "size_t", "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t",
                           "uint64_t", "std", "string", "vector", "vec2", "vec3", "vec4", "mat3", "mat4", "float2",
                           "float3", "float4", "float4x4", "wchar_t", "char8_t", "nullptr_t"};
                c.lineComment = "//";
                c.blockOpen = "/*";
                c.blockClose = "*/";
                c.hashMeta = true;
                list.push_back(c);

                LanguageSpec cs;
                cs.names = {"cs", "csharp", "c#", "java", "kotlin", "kt", "swift", "dart", "scala"};
                cs.keywords = {"abstract", "as", "async", "await", "base", "break", "case", "catch", "class", "const",
                               "continue", "default", "delegate", "do", "else", "enum", "event", "explicit", "extends",
                               "extern", "final", "finally", "for", "foreach", "fun", "func", "get", "guard", "if",
                               "implements", "import", "in", "interface", "internal", "is", "let", "lock", "namespace",
                               "new", "object", "operator", "out", "override", "package", "params", "private",
                               "protected", "public", "readonly", "record", "ref", "return", "sealed", "set", "static",
                               "struct", "super", "switch", "this", "throw", "throws", "try", "typeof", "using", "val",
                               "var", "virtual", "when", "where", "while", "yield"};
                cs.types = {"void", "bool", "boolean", "byte", "char", "decimal", "double", "float", "int", "long",
                            "sbyte", "short", "string", "String", "uint", "ulong", "ushort", "Int", "Double", "Float"};
                cs.lineComment = "//";
                cs.blockOpen = "/*";
                cs.blockClose = "*/";
                cs.atMeta = true;
                cs.tripleQuotes = true;
                list.push_back(cs);

                LanguageSpec js;
                js.names = {"js", "javascript", "jsx", "ts", "typescript", "tsx", "mjs", "cjs"};
                js.keywords = {"async", "await", "break", "case", "catch", "class", "const", "continue", "debugger",
                               "default", "delete", "do", "else", "export", "extends", "finally", "for", "from",
                               "function", "if", "import", "in", "instanceof", "interface", "let", "new", "of",
                               "return", "static", "super", "switch", "this", "throw", "try", "type", "typeof", "var",
                               "void", "while", "with", "yield", "enum", "implements", "private", "public",
                               "protected", "readonly", "as", "keyof", "declare", "namespace"};
                js.types = {"string", "number", "boolean", "any", "unknown", "never", "object", "symbol", "bigint"};
                js.lineComment = "//";
                js.blockOpen = "/*";
                js.blockClose = "*/";
                js.backtickStrings = true;
                js.atMeta = true;
                list.push_back(js);

                LanguageSpec go;
                go.names = {"go", "golang"};
                go.keywords = {"break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough",
                               "for", "func", "go", "goto", "if", "import", "interface", "map", "package", "range",
                               "return", "select", "struct", "switch", "type", "var"};
                go.types = {"bool", "byte", "complex64", "complex128", "error", "float32", "float64", "int", "int8",
                            "int16", "int32", "int64", "rune", "string", "uint", "uint8", "uint16", "uint32",
                            "uint64", "uintptr", "any"};
                go.lineComment = "//";
                go.blockOpen = "/*";
                go.blockClose = "*/";
                go.backtickStrings = true;
                list.push_back(go);

                LanguageSpec rust;
                rust.names = {"rs", "rust", "zig"};
                rust.keywords = {"as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
                                 "extern", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod", "move",
                                 "mut", "pub", "ref", "return", "self", "Self", "static", "struct", "super", "trait",
                                 "type", "unsafe", "use", "where", "while", "defer", "errdefer", "try", "comptime"};
                rust.types = {"bool", "char", "f32", "f64", "i8", "i16", "i32", "i64", "i128", "isize", "str",
                              "u8", "u16", "u32", "u64", "u128", "usize", "String", "Vec", "Option", "Result", "Box"};
                rust.lineComment = "//";
                rust.blockOpen = "/*";
                rust.blockClose = "*/";
                rust.atMeta = true;
                list.push_back(rust);

                LanguageSpec py;
                py.names = {"py", "python", "python3", "gd", "gdscript"};
                py.keywords = {"and", "as", "assert", "async", "await", "break", "class", "continue", "def", "del",
                               "elif", "else", "except", "finally", "for", "from", "global", "if", "import", "in",
                               "is", "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try", "while",
                               "with", "yield", "match", "case", "func", "extends", "var", "signal", "export"};
                py.types = {"int", "float", "str", "bool", "list", "dict", "set", "tuple", "bytes", "object",
                            "self", "cls"};
                py.lineComment = "#";
                py.tripleQuotes = true;
                py.atMeta = true;
                list.push_back(py);

                LanguageSpec lua;
                lua.names = {"lua", "luau"};
                lua.keywords = {"and", "break", "do", "else", "elseif", "end", "for", "function", "goto", "if", "in",
                                "local", "not", "or", "repeat", "return", "then", "until", "while"};
                lua.types = {"slate", "string", "table", "math", "os", "utf8", "coroutine", "self"};
                lua.lineComment = "--";
                lua.luaStrings = true;
                lua.capitalizedTypes = false;
                list.push_back(lua);

                LanguageSpec ruby;
                ruby.names = {"rb", "ruby", "elixir", "ex", "exs"};
                ruby.keywords = {"alias", "and", "begin", "break", "case", "class", "def", "defined?", "do", "else",
                                 "elsif", "end", "ensure", "for", "if", "in", "module", "next", "not", "or", "redo",
                                 "rescue", "retry", "return", "self", "super", "then", "undef", "unless", "until",
                                 "when", "while", "yield", "defmodule", "defp", "fn"};
                ruby.lineComment = "#";
                ruby.atMeta = true;
                list.push_back(ruby);

                LanguageSpec sh;
                sh.names = {"sh", "bash", "zsh", "shell", "console", "fish", "bat", "cmd"};
                sh.keywords = {"if", "then", "else", "elif", "fi", "for", "while", "do", "done", "case", "esac",
                               "function", "in", "return", "export", "local", "echo", "cd", "set", "unset", "source",
                               "exit", "sudo", "git", "cmake", "npm", "cargo", "make", "rem", "goto", "call"};
                sh.lineComment = "#";
                sh.dollarVariables = true;
                sh.capitalizedTypes = false;
                list.push_back(sh);

                LanguageSpec ps;
                ps.names = {"ps", "ps1", "powershell", "pwsh"};
                ps.keywords = {"if", "else", "elseif", "foreach", "for", "while", "do", "switch", "function", "param",
                               "return", "try", "catch", "finally", "throw", "begin", "process", "end", "in"};
                ps.lineComment = "#";
                ps.blockOpen = "<#";
                ps.blockClose = "#>";
                ps.dollarVariables = true;
                ps.caseInsensitive = true;
                list.push_back(ps);

                LanguageSpec sql;
                sql.names = {"sql", "sqlite", "postgres", "mysql"};
                sql.keywords = {"select", "from", "where", "insert", "into", "values", "update", "set", "delete",
                                "create", "table", "drop", "alter", "add", "join", "left", "right", "inner", "outer",
                                "on", "group", "by", "order", "having", "limit", "offset", "as", "and", "or", "not",
                                "null", "is", "in", "like", "primary", "key", "foreign", "references", "index",
                                "distinct", "union", "all", "case", "when", "then", "else", "end", "with", "returning"};
                sql.types = {"int", "integer", "text", "varchar", "char", "real", "float", "double", "boolean",
                             "date", "timestamp", "blob", "serial", "bigint", "numeric"};
                sql.lineComment = "--";
                sql.blockOpen = "/*";
                sql.blockClose = "*/";
                sql.caseInsensitive = true;
                sql.capitalizedTypes = false;
                list.push_back(sql);

                LanguageSpec json;
                json.names = {"json", "jsonc", "json5"};
                json.lineComment = "//";
                json.singleQuoteStrings = false;
                json.capitalizedTypes = false;
                json.keyValue = true;
                list.push_back(json);

                LanguageSpec yaml;
                yaml.names = {"yaml", "yml", "toml", "ini", "cfg", "conf", "properties", "env", "tsv"};
                yaml.lineComment = "#";
                yaml.lineComment2 = ";";
                yaml.capitalizedTypes = false;
                yaml.keyValue = true;
                list.push_back(yaml);

                LanguageSpec css;
                css.names = {"css", "scss", "sass", "less"};
                css.keywords = {"important", "media", "import", "keyframes", "from", "to", "root", "hover", "active",
                                "focus", "before", "after"};
                css.blockOpen = "/*";
                css.blockClose = "*/";
                css.lineComment = "//";
                css.atMeta = true;
                css.capitalizedTypes = false;
                css.keyValue = true;
                list.push_back(css);

                LanguageSpec html;
                html.names = {"html", "htm", "xml", "svg", "xaml", "vue", "svelte", "md", "markdown"};
                html.blockOpen = "<!--";
                html.blockClose = "-->";
                html.markup = true;
                html.capitalizedTypes = false;
                list.push_back(html);

                return list;
            }();
            return specs;
        }

        const LanguageSpec* FindSpec(std::string_view language)
        {
            std::string lower(language);
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            for (const auto& spec : Specs())
            {
                for (const char* name : spec.names)
                {
                    if (lower == name)
                    {
                        return &spec;
                    }
                }
            }
            return nullptr;
        }

        bool IsIdentStart(char ch)
        {
            const auto byte = static_cast<unsigned char>(ch);
            return std::isalpha(byte) != 0 || ch == '_' || byte >= 0x80;
        }

        bool IsIdentChar(char ch)
        {
            const auto byte = static_cast<unsigned char>(ch);
            return std::isalnum(byte) != 0 || ch == '_' || byte >= 0x80;
        }

        bool StartsWith(std::string_view text, std::size_t at, const char* prefix)
        {
            if (!prefix)
            {
                return false;
            }
            const std::string_view p(prefix);
            return text.substr(at, p.size()) == p;
        }

        bool IsConstantWord(const std::string& word)
        {
            static const std::unordered_set<std::string> constants{
                "true", "false", "null", "nullptr", "nil", "None", "True", "False", "undefined", "NaN", "Infinity",
                "TRUE", "FALSE", "NULL", "yes", "no", "on", "off"};
            return constants.count(word) != 0;
        }

        class Painter
        {
        public:
            Painter(std::string_view line, std::vector<CodeToken>* tokens) : m_line(line), m_tokens(tokens)
            {
                tokens->assign(line.size(), CodeToken::Text);
            }

            void Fill(std::size_t start, std::size_t end, CodeToken token)
            {
                end = std::min(end, m_line.size());
                for (std::size_t i = start; i < end; ++i)
                {
                    (*m_tokens)[i] = token;
                }
            }

        private:
            std::string_view m_line;
            std::vector<CodeToken>* m_tokens;
        };

        // Scans a quoted string starting at i (the opening quote) and returns the end offset
        std::size_t ScanString(std::string_view line, std::size_t i, char quote)
        {
            std::size_t end = i + 1;
            while (end < line.size())
            {
                if (line[end] == '\\')
                {
                    end += 2;
                    continue;
                }
                if (line[end] == quote)
                {
                    return end + 1;
                }
                ++end;
            }
            return line.size();
        }

        // Returns the level of a Lua long bracket like [[ or [==[, or -1
        int LuaLongBracketLevel(std::string_view line, std::size_t i)
        {
            if (i >= line.size() || line[i] != '[')
            {
                return -1;
            }
            std::size_t j = i + 1;
            int level = 0;
            while (j < line.size() && line[j] == '=')
            {
                ++level;
                ++j;
            }
            return (j < line.size() && line[j] == '[') ? level : -1;
        }
    }

    bool IsKnownCodeLanguage(std::string_view language)
    {
        return FindSpec(language) != nullptr;
    }

    std::vector<CodeToken> HighlightCodeLine(std::string_view language, std::string_view line, int* state)
    {
        std::vector<CodeToken> tokens;
        Painter painter(line, &tokens);
        const LanguageSpec* spec = FindSpec(language);
        int dummyState = 0;
        int& carry = state ? *state : dummyState;
        if (!spec)
        {
            return tokens;
        }

        std::size_t i = 0;

        // Continue constructs that span lines
        const auto closeCarry = [&](const char* close, CodeToken token) {
            const std::size_t end = line.find(close);
            if (end == std::string_view::npos)
            {
                painter.Fill(0, line.size(), token);
                i = line.size();
                return;
            }
            const std::size_t stop = end + std::string_view(close).size();
            painter.Fill(0, stop, token);
            i = stop;
            carry = 0;
        };
        switch (carry)
        {
        case StateBlockComment:
            closeCarry(spec->blockClose ? spec->blockClose : "*/", CodeToken::Comment);
            break;
        case StateTripleDouble:
            closeCarry("\"\"\"", CodeToken::String);
            break;
        case StateTripleSingle:
            closeCarry("'''", CodeToken::String);
            break;
        case StateBacktick:
            closeCarry("`", CodeToken::String);
            break;
        case StateLuaLongString:
            closeCarry("]]", CodeToken::String);
            break;
        case StateLuaLongComment:
            closeCarry("]]", CodeToken::Comment);
            break;
        default:
            break;
        }

        // Preprocessor lines and key/value configs are line-shaped
        if (i == 0 && spec->hashMeta)
        {
            std::size_t first = line.find_first_not_of(" \t");
            if (first != std::string_view::npos && line[first] == '#')
            {
                std::size_t end = first + 1;
                while (end < line.size() && IsIdentChar(line[end]))
                {
                    ++end;
                }
                painter.Fill(first, end, CodeToken::Meta);
                i = end;
            }
        }
        if (i == 0 && spec->keyValue && !spec->markup)
        {
            const std::size_t first = line.find_first_not_of(" \t-");
            if (first != std::string_view::npos && line[first] == '[' && language != "json")
            {
                painter.Fill(first, line.size(), CodeToken::Meta);
                return tokens;
            }
        }

        while (i < line.size())
        {
            const char ch = line[i];

            if (StartsWith(line, i, spec->lineComment) || StartsWith(line, i, spec->lineComment2))
            {
                if (spec->luaStrings && LuaLongBracketLevel(line, i + 2) >= 0)
                {
                    const std::size_t close = line.find("]]", i + 4);
                    if (close == std::string_view::npos)
                    {
                        painter.Fill(i, line.size(), CodeToken::Comment);
                        carry = StateLuaLongComment;
                        return tokens;
                    }
                    painter.Fill(i, close + 2, CodeToken::Comment);
                    i = close + 2;
                    continue;
                }
                painter.Fill(i, line.size(), CodeToken::Comment);
                return tokens;
            }
            if (StartsWith(line, i, spec->blockOpen))
            {
                const std::size_t openLength = std::string_view(spec->blockOpen).size();
                const std::size_t close = line.find(spec->blockClose, i + openLength);
                if (close == std::string_view::npos)
                {
                    painter.Fill(i, line.size(), CodeToken::Comment);
                    carry = StateBlockComment;
                    return tokens;
                }
                const std::size_t stop = close + std::string_view(spec->blockClose).size();
                painter.Fill(i, stop, CodeToken::Comment);
                i = stop;
                continue;
            }

            if (spec->markup && ch == '<')
            {
                std::size_t end = i + 1;
                if (end < line.size() && (line[end] == '/' || line[end] == '!' || line[end] == '?'))
                {
                    ++end;
                }
                const std::size_t nameStart = end;
                while (end < line.size() && (IsIdentChar(line[end]) || line[end] == '-' || line[end] == ':'))
                {
                    ++end;
                }
                painter.Fill(i, nameStart, CodeToken::Punctuation);
                painter.Fill(nameStart, end, CodeToken::Tag);
                // Attributes until the closing bracket
                while (end < line.size() && line[end] != '>')
                {
                    if (line[end] == '"' || line[end] == '\'')
                    {
                        const std::size_t stop = ScanString(line, end, line[end]);
                        painter.Fill(end, stop, CodeToken::String);
                        end = stop;
                        continue;
                    }
                    if (IsIdentStart(line[end]))
                    {
                        std::size_t stop = end;
                        while (stop < line.size() && (IsIdentChar(line[stop]) || line[stop] == '-' || line[stop] == ':'))
                        {
                            ++stop;
                        }
                        painter.Fill(end, stop, CodeToken::Property);
                        end = stop;
                        continue;
                    }
                    ++end;
                }
                if (end < line.size())
                {
                    painter.Fill(end, end + 1, CodeToken::Punctuation);
                    ++end;
                }
                i = end;
                continue;
            }
            if (spec->markup)
            {
                ++i;
                continue;
            }

            if (spec->tripleQuotes && (StartsWith(line, i, "\"\"\"") || StartsWith(line, i, "'''")))
            {
                const char* quote = line[i] == '"' ? "\"\"\"" : "'''";
                const std::size_t close = line.find(quote, i + 3);
                if (close == std::string_view::npos)
                {
                    painter.Fill(i, line.size(), CodeToken::String);
                    carry = line[i] == '"' ? StateTripleDouble : StateTripleSingle;
                    return tokens;
                }
                painter.Fill(i, close + 3, CodeToken::String);
                i = close + 3;
                continue;
            }
            if (spec->luaStrings && LuaLongBracketLevel(line, i) >= 0)
            {
                const std::size_t close = line.find("]]", i + 2);
                if (close == std::string_view::npos)
                {
                    painter.Fill(i, line.size(), CodeToken::String);
                    carry = StateLuaLongString;
                    return tokens;
                }
                painter.Fill(i, close + 2, CodeToken::String);
                i = close + 2;
                continue;
            }
            if (ch == '"' || (ch == '\'' && spec->singleQuoteStrings) || (ch == '`' && spec->backtickStrings))
            {
                if (ch == '`')
                {
                    const std::size_t close = line.find('`', i + 1);
                    if (close == std::string_view::npos)
                    {
                        painter.Fill(i, line.size(), CodeToken::String);
                        carry = StateBacktick;
                        return tokens;
                    }
                    painter.Fill(i, close + 1, CodeToken::String);
                    i = close + 1;
                    continue;
                }
                const std::size_t end = ScanString(line, i, ch);
                // A string followed by ':' is a key in JSON-like data
                std::size_t after = end;
                while (after < line.size() && (line[after] == ' ' || line[after] == '\t'))
                {
                    ++after;
                }
                const bool isKey = spec->keyValue && after < line.size() && line[after] == ':';
                painter.Fill(i, end, isKey ? CodeToken::Property : CodeToken::String);
                i = end;
                continue;
            }

            if (std::isdigit(static_cast<unsigned char>(ch)) != 0 ||
                (ch == '.' && i + 1 < line.size() && std::isdigit(static_cast<unsigned char>(line[i + 1])) != 0))
            {
                if (i > 0 && IsIdentChar(line[i - 1]))
                {
                    ++i;
                    continue;
                }
                std::size_t end = i + 1;
                while (end < line.size() && (std::isalnum(static_cast<unsigned char>(line[end])) != 0 ||
                                             line[end] == '.' || line[end] == '_' || line[end] == '\''))
                {
                    ++end;
                }
                painter.Fill(i, end, CodeToken::Number);
                i = end;
                continue;
            }

            if ((spec->atMeta && ch == '@') || (spec->dollarVariables && ch == '$'))
            {
                std::size_t end = i + 1;
                if (end < line.size() && line[end] == '{')
                {
                    const std::size_t close = line.find('}', end);
                    end = close == std::string_view::npos ? line.size() : close + 1;
                }
                else
                {
                    while (end < line.size() && (IsIdentChar(line[end]) || line[end] == '.'))
                    {
                        ++end;
                    }
                }
                painter.Fill(i, end, ch == '@' ? CodeToken::Meta : CodeToken::Constant);
                i = end;
                continue;
            }

            if (IsIdentStart(ch))
            {
                std::size_t end = i + 1;
                while (end < line.size() && (IsIdentChar(line[end]) || (spec->caseInsensitive && line[end] == '-')))
                {
                    ++end;
                }
                std::string word(line.substr(i, end - i));
                std::string lookup = word;
                if (spec->caseInsensitive)
                {
                    std::transform(lookup.begin(), lookup.end(), lookup.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                    });
                }

                std::size_t after = end;
                while (after < line.size() && line[after] == ' ')
                {
                    ++after;
                }
                const bool calls = after < line.size() && line[after] == '(';
                const bool keyed = spec->keyValue && after < line.size() && (line[after] == ':' || line[after] == '=') &&
                                   line.find_first_not_of(" \t-") == i;

                CodeToken token = CodeToken::Text;
                if (spec->keywords.count(lookup) != 0)
                {
                    token = CodeToken::Keyword;
                }
                else if (IsConstantWord(word))
                {
                    token = CodeToken::Constant;
                }
                else if (keyed)
                {
                    token = CodeToken::Property;
                }
                else if (spec->types.count(lookup) != 0)
                {
                    token = CodeToken::Type;
                }
                else if (calls)
                {
                    token = CodeToken::Function;
                }
                else if (spec->capitalizedTypes && std::isupper(static_cast<unsigned char>(word[0])) != 0)
                {
                    token = CodeToken::Type;
                }
                painter.Fill(i, end, token);
                i = end;
                continue;
            }

            if (std::ispunct(static_cast<unsigned char>(ch)) != 0)
            {
                painter.Fill(i, i + 1, CodeToken::Punctuation);
            }
            ++i;
        }
        return tokens;
    }
}
