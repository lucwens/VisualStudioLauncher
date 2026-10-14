#include "../src/Core.h"
#include <iostream>
#include <stdexcept>

using namespace launcher;

void Require(bool condition, const char *message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void Reject(const std::string &json)
{
    try
    {
        ParseSettings(json);
    }
    catch (const std::exception &)
    {
        return;
    }
    throw std::runtime_error("Malformed settings were accepted");
}

int main()
{
    try
    {
        Settings original;
        original.visualStudio = R"(C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe)";
        Entry main;
        main.id = "ctrack-main";
        main.name = u8"CTrack — België \"main\"";
        main.group = "CTrack";
        main.branch = "main";
        main.path = R"(D:\_Projects\CTrack Main\CTrack.sln)";
        main.color = "#16A34A";
        main.icon = R"(C:\Icons\Green.ico)";
        original.entries.push_back(main);
        Entry dev = main;
        dev.id = "ctrack-dev";
        dev.branch = "feature/robot";
        dev.path = R"(\\server\shared projects\CTrack.slnx)";
        dev.enabled = false;
        original.entries.push_back(dev);
        const auto restored = ParseSettings(SerializeSettings(original));
        Require(restored.visualStudio == original.visualStudio, "VS path changed");
        Require(restored.entries.size() == 2, "Entries lost");
        Require(restored.entries[0].name == main.name && restored.entries[0].path == main.path,
                "Unicode name or spaced path changed");
        Require(restored.entries[1].path == dev.path && !restored.entries[1].enabled, "UNC path or visibility changed");
        Require(restored.entries[0].id == main.id && restored.entries[1].id == dev.id, "Stable IDs or order changed");
        Require(DisplayName(main) == main.name + " [main]", "Branch label missing");
        Require(!restored.entries[0].branchFromGit, "Manual branch source changed");
        Settings fromGit;
        fromGit.entries.push_back(main);
        fromGit.entries[0].branchFromGit = true;
        Require(ParseSettings(SerializeSettings(fromGit)).entries[0].branchFromGit, "Git branch source lost");
        Require(BranchFromHead("ref: refs/heads/feature/robot\n") == "feature/robot", "Branch HEAD misread");
        Require(BranchFromHead("0123456789abcdef0123456789abcdef01234567\r\n") == "0123456", "Detached HEAD misread");
        Require(BranchFromHead("garbage").empty() && BranchFromHead("").empty(), "Invalid HEAD accepted");
        Reject(R"({"version":1,"entries":[{"id":"a","branchFromGit":"yes"}]})");
        const auto unicode = ParseSettings(R"({"version":1,"entries":[{"id":"a","name":"\u00e9 \ud83d\ude80"}]})");
        Require(unicode.entries[0].name == u8"é 🚀", "Unicode escape pair decoded incorrectly");
        Reject(R"({"version":2,"entries":[]})");
        Reject(R"({"version":1,"entries":[{"id":"a"},{"id":"a"}]})");
        Reject(R"({"version":1,"entries":[{"id":"a --manage"}]})");
        Reject(R"({"version":1,"entries":[{"id":"a","color":"red"}]})");
        Reject(R"({"version":1,"entries":[{"id":"a","enabled":"yes"}]})");
        Reject(R"({"version":1,"entries":[{"id":"a","name":"\ud83d"}]})");
        Reject(R"({"version":1,"version":2,"entries":[]})");
        Reject(R"({"version":1,"entries":[],})");
        Reject(R"({"version":1,"entries":[]} trailing)");
        Reject(R"({"version":1,"entries":[)");
        Require(IsSolutionPath("D:\\a.SLNX") && IsSolutionPath("D:\\a.sln"), "Solution format rejected");
        Require(!IsSolutionPath("D:\\a.sln.exe"), "Executable accepted as solution");
        Require(QuoteArgument(L"C:\\A B\\C.sln") == L"\"C:\\A B\\C.sln\"", "Spaced argument not quoted");
        Require(QuoteArgument(L"C:\\A B\\") == L"\"C:\\A B\\\\\"", "Trailing slash not escaped");
        Require(QuoteArgument(L"a\"b") == L"\"a\\\"b\"", "Quote not escaped");
        Require(QuoteArgument(L"") == L"\"\"", "Empty argument changed");
        const auto recent = ParseRecentSolutions(
            R"([{"Key":"a","Value":{"LocalProperties":{"FullPath":"D:\\old\\A.sln","Type":0},"LastAccessed":"2026-01-01T00:00:00+00:00"}},)"
            R"({"Key":"b","Value":{"LocalProperties":{"FullPath":"D:\\dir","Type":1},"LastAccessed":"2026-09-01T00:00:00+00:00"}},)"
            R"({"Key":"c","Value":{"LocalProperties":{"FullPath":"D:\\p.vcxproj","Type":0},"LastAccessed":"2026-09-01T00:00:00+00:00"}},)"
            R"({"Key":"d","Value":{"LocalProperties":{"FullPath":"D:\\new\\B.slnx","Type":0},"LastAccessed":"2026-05-01T00:00:00+00:00"}}])");
        Require(recent.size() == 2, "Recent list kept folders or projects");
        Require(recent[0].path == R"(D:\new\B.slnx)" && recent[1].path == R"(D:\old\A.sln)",
                "Recent list order wrong");
        std::cout << "All configuration, Unicode, stable-ID, and Windows argument checks passed.\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
