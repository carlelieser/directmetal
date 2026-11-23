#include <directmetal/runtime/config.hpp>

#include "log_fixture.hpp"

#include <array>
#include <set>

namespace {

using namespace directmetal::config;
namespace log = directmetal::log;

constexpr std::array<std::string_view, 3> quality_choices = {"low", "Medium", "high"};
constexpr std::array<OptionSpec, 5> test_registry = {{
    {"flag", "false", "A boolean.", OptionType::boolean, {}},
    {"count", "7", "An integer.", OptionType::integer, {}},
    {"name", "plain", "A string.", OptionType::string, {}},
    {"quality", "Medium", "A choice.", OptionType::choice, quality_choices},
    {"other", "base", "A second string.", OptionType::string, {}},
}};

class ConfigTest : public log::testing_support::LogFixture {
protected:
    Config resolve(const Sources& sources) { return Config::resolve(sources, test_registry); }

    Config from_file(std::string text, std::string application = "game.exe") {
        return resolve({std::move(application), std::move(text), std::nullopt});
    }

    Config from_overrides(std::string text) { return resolve({"game.exe", std::nullopt, text}); }

    std::vector<std::string> warnings() const {
        std::vector<std::string> messages;
        for (const auto& line : sink.lines) {
            EXPECT_EQ(line.level, log::Level::warn);
            EXPECT_EQ(line.component, log::Component::runtime);
            messages.push_back(line.message);
        }
        return messages;
    }
};

TEST_F(ConfigTest, DefaultsWithoutSources) {
    const Config config = resolve({"game.exe", std::nullopt, std::nullopt});
    EXPECT_FALSE(config.get_bool("flag"));
    EXPECT_EQ(config.get_int("count"), 7);
    EXPECT_EQ(config.get_string("name"), "plain");
    EXPECT_EQ(config.get_string("quality"), "Medium");
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, DefaultConstructedUsesRegistryDefaults) {
    const Config config;
    EXPECT_FALSE(config.get_bool("hud"));
}

TEST_F(ConfigTest, EverywhereSectionOverridesDefault) {
    const Config config = from_file("[*]\ncount = 9\n");
    EXPECT_EQ(config.get_int("count"), 9);
    EXPECT_EQ(config.get_string("name"), "plain");
}

TEST_F(ConfigTest, ApplicationSectionOverridesEverywhereRegardlessOfOrder) {
    const Config config = from_file("[app:game.exe]\nname = app\n[*]\nname = star\n");
    EXPECT_EQ(config.get_string("name"), "app");
}

TEST_F(ConfigTest, OverridesBeatApplicationSection) {
    const Config config =
        resolve({"game.exe", "[*]\nname=star\n[app:game.exe]\nname=app\n", "name=env"});
    EXPECT_EQ(config.get_string("name"), "env");
}

TEST_F(ConfigTest, EachKeyTakesItsHighestPrecedenceSource) {
    const Config config = resolve({"game.exe",
                                   "[*]\ncount=1\nname=star\nother=star\n"
                                   "[app:game.exe]\nname=app\nother=app\n",
                                   "other=env"});
    EXPECT_EQ(config.get_int("count"), 1);
    EXPECT_EQ(config.get_string("name"), "app");
    EXPECT_EQ(config.get_string("other"), "env");
    EXPECT_EQ(config.get_bool("flag"), false);
}

TEST_F(ConfigTest, OtherApplicationSectionsAreIgnored) {
    const Config config = from_file("[app:other.exe]\nname = theirs\n[app:game.exe.bak]\nname=x\n");
    EXPECT_EQ(config.get_string("name"), "plain");
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, ApplicationNameIgnoresCase) {
    EXPECT_EQ(from_file("[app:GAME.Exe]\nname = hit\n", "game.exe").get_string("name"), "hit");
    EXPECT_EQ(from_file("[APP:game.exe]\nname = hit\n", "GAME.EXE").get_string("name"), "hit");
}

TEST_F(ConfigTest, ApplicationNameMustMatchExtension) {
    EXPECT_EQ(from_file("[app:game]\nname = miss\n", "game.exe").get_string("name"), "plain");
}

TEST_F(ConfigTest, ApplicationNameBlanksAreTrimmed) {
    EXPECT_EQ(from_file("[ app: game.exe ]\nname = hit\n").get_string("name"), "hit");
}

TEST_F(ConfigTest, LaterAssignmentsAndRepeatedSectionsWin) {
    const Config config = from_file(
        "[app:game.exe]\nname=one\n[*]\nname=star\n[app:game.exe]\nname=two\nname=three\n");
    EXPECT_EQ(config.get_string("name"), "three");
}

TEST_F(ConfigTest, KeysIgnoreCaseAndBlanks) {
    const Config config = from_file("[*]\n   COUNT\t=   12   \n");
    EXPECT_EQ(config.get_int("count"), 12);
    EXPECT_EQ(config.get_int("COUNT"), 12);
}

TEST_F(ConfigTest, CommentsBlankLinesAndCarriageReturns) {
    const Config config =
        from_file("# top\r\n\r\n[*]\r\n; semi\r\nname = a # b\r\n  # indented\r\n");
    EXPECT_EQ(config.get_string("name"), "a # b");
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, FileWithoutTrailingNewline) {
    EXPECT_EQ(from_file("[*]\nname=last").get_string("name"), "last");
}

TEST_F(ConfigTest, LeadingUtf8ByteOrderMarkIsIgnored) {
    const Config config = from_file("\xEF\xBB\xBF[*]\nname = marked\n");
    EXPECT_EQ(config.get_string("name"), "marked");
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, ByteOrderMarkOnlyCountsAtTheStart) {
    const Config config = from_file("[*]\n\xEF\xBB\xBFname = late\n");
    EXPECT_EQ(config.get_string("name"), "plain");
    EXPECT_EQ(sink.lines.size(), 1u);
}

TEST_F(ConfigTest, VeryLongLinesAreHandled) {
    const std::string huge(1 << 20, 'x');
    const Config config = from_file("[*]\nname = " + huge + "\n" + huge + "\ncount = 3\n");
    EXPECT_EQ(config.get_string("name"), huge);
    EXPECT_EQ(config.get_int("count"), 3);
    EXPECT_EQ(sink.lines.size(), 1u);
}

TEST_F(ConfigTest, DuplicateKeyLastValidValueWins) {
    const Config config = from_file("[*]\ncount = 1\ncount = 2\ncount = nope\n");
    EXPECT_EQ(config.get_int("count"), 2);
    EXPECT_EQ(sink.lines.size(), 1u);
}

TEST_F(ConfigTest, DuplicateKeyAcrossOverridesLastWins) {
    EXPECT_EQ(from_overrides("count=1;count=2").get_int("count"), 2);
}

TEST_F(ConfigTest, EmptyValueIsAValue) {
    EXPECT_EQ(from_file("[*]\nname =\n").get_string("name"), "");
}

TEST_F(ConfigTest, MalformedLinesWarnWithTheirLineNumber) {
    const Config config = from_file("[*]\nnot an assignment\n=value\nname=ok\n");
    EXPECT_EQ(config.get_string("name"), "ok");
    EXPECT_EQ(warnings(), (std::vector<std::string>{
                              "config file line 2: expected key=value, got \"not an assignment\"",
                              "config file line 3: expected key=value, got \"=value\"",
                          }));
}

TEST_F(ConfigTest, KeyBeforeAnySectionWarns) {
    const Config config = from_file("name=early\n[*]\ncount=2\n");
    EXPECT_EQ(config.get_string("name"), "plain");
    EXPECT_EQ(config.get_int("count"), 2);
    EXPECT_EQ(warnings(), std::vector<std::string>{
                              "config file line 1: \"name=early\" is outside any section"});
}

TEST_F(ConfigTest, UnknownSectionsWarnOnceAndSkipTheirKeys) {
    const Config config = from_file("[*]\nname=star\n[other]\nname=bad\n[app:]\nname=worse\n[*\n");
    EXPECT_EQ(config.get_string("name"), "star");
    EXPECT_EQ(warnings(), (std::vector<std::string>{
                              "config file line 3: unknown section \"[other]\"",
                              "config file line 5: unknown section \"[app:]\"",
                              "config file line 7: unknown section \"[*\"",
                          }));
}

TEST_F(ConfigTest, UnknownKeysWarnInEverySectionAndAreIgnored) {
    const Config config = from_file("[*]\nbogus = 1\n[app:other.exe]\nmystery = 2\nname=ok\n");
    EXPECT_EQ(config.get_string("name"), "plain");
    EXPECT_EQ(warnings(), (std::vector<std::string>{
                              "config file line 2: unknown option \"bogus\"",
                              "config file line 4: unknown option \"mystery\"",
                          }));
}

TEST_F(ConfigTest, OverrideEntriesSplitOnSemicolons) {
    const Config config = from_overrides(" count=3 ;; name = spaced out ;flag=on;");
    EXPECT_EQ(config.get_int("count"), 3);
    EXPECT_EQ(config.get_string("name"), "spaced out");
    EXPECT_TRUE(config.get_bool("flag"));
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, MalformedAndUnknownOverrideEntriesWarn) {
    const Config config = from_overrides("count=3;nonsense;bogus=1;name=ok");
    EXPECT_EQ(config.get_int("count"), 3);
    EXPECT_EQ(config.get_string("name"), "ok");
    EXPECT_EQ(warnings(), (std::vector<std::string>{
                              "DIRECTMETAL_CONFIG entry 2: expected key=value, got \"nonsense\"",
                              "DIRECTMETAL_CONFIG entry 3: unknown option \"bogus\"",
                          }));
}

TEST_F(ConfigTest, InvalidOverrideFallsBackToTheFile) {
    const Config config = resolve({"game.exe", "[*]\ncount=4\n", "count=many"});
    EXPECT_EQ(config.get_int("count"), 4);
    EXPECT_EQ(warnings(), std::vector<std::string>{"DIRECTMETAL_CONFIG entry 1: invalid integer "
                                                   "value \"many\" for option \"count\""});
}

TEST_F(ConfigTest, InvalidApplicationValueFallsBackToEverywhere) {
    const Config config = from_file("[*]\nflag=yes\n[app:game.exe]\nflag=maybe\n");
    EXPECT_TRUE(config.get_bool("flag"));
    EXPECT_EQ(warnings(), std::vector<std::string>{"config file line 4: invalid boolean value "
                                                   "\"maybe\" for option \"flag\""});
}

TEST_F(ConfigTest, BooleanSpellings) {
    for (const char* truthy : {"true", "TRUE", "yes", "On", "1"}) {
        EXPECT_TRUE(from_overrides(std::string("flag=") + truthy).get_bool("flag")) << truthy;
    }
    for (const char* falsy : {"false", "No", "OFF", "0"}) {
        const Config config =
            resolve({"game.exe", "[*]\nflag=true\n", std::string("flag=") + falsy});
        EXPECT_FALSE(config.get_bool("flag")) << falsy;
    }
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, IntegerForms) {
    EXPECT_EQ(from_overrides("count=-12").get_int("count"), -12);
    EXPECT_EQ(from_overrides("count=+5").get_int("count"), 5);
    EXPECT_EQ(from_overrides("count=007").get_int("count"), 7);
    EXPECT_EQ(from_overrides("count=9223372036854775807").get_int("count"),
              std::numeric_limits<std::int64_t>::max());
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, InvalidIntegersAreRejected) {
    for (const char* bad : {"1.5", "0x10", "12abc", "", "+-3", "9223372036854775808", "--1"}) {
        const Config config = from_overrides(std::string("count=") + bad);
        EXPECT_EQ(config.get_int("count"), 7) << bad;
    }
    EXPECT_EQ(sink.lines.size(), 7u);
}

TEST_F(ConfigTest, ChoiceMatchesIgnoringCaseAndReturnsTheCanonicalSpelling) {
    EXPECT_EQ(from_overrides("quality=HIGH").get_string("quality"), "high");
    EXPECT_EQ(from_overrides("quality=medium").get_string("quality"), "Medium");
    EXPECT_TRUE(sink.lines.empty());
}

TEST_F(ConfigTest, UnknownChoiceKeepsTheDefault) {
    EXPECT_EQ(from_overrides("quality=ultra").get_string("quality"), "Medium");
    EXPECT_EQ(warnings(),
              std::vector<std::string>{"DIRECTMETAL_CONFIG entry 1: invalid choice value "
                                       "\"ultra\" for option \"quality\""});
}

TEST_F(ConfigTest, AccessorsRejectUnknownKeysAndWrongTypes) {
    const Config config = resolve({"game.exe", std::nullopt, "flag=true;count=5"});
    EXPECT_FALSE(config.get_bool("count"));
    EXPECT_EQ(config.get_int("flag"), 0);
    EXPECT_EQ(config.get_string("missing"), "");
    ASSERT_EQ(sink.lines.size(), 3u);
    for (const auto& line : sink.lines) {
        EXPECT_EQ(line.level, log::Level::error);
    }
    EXPECT_EQ(sink.lines[0].message, "config: no boolean option \"count\"");
}

TEST_F(ConfigTest, WarningsEchoAtMostAnExcerptOfALongLine) {
    from_file("[*]\n" + std::string(1 << 20, 'x') + "\n");
    from_overrides(std::string(5000, 'k') + "=1");
    from_overrides("count=" + std::string(5000, '9'));
    ASSERT_EQ(sink.lines.size(), 3u);
    for (const auto& line : sink.lines) {
        EXPECT_LT(line.message.size(), 300u);
        EXPECT_TRUE(line.message.find("...\"") != std::string::npos) << line.message;
    }
}

TEST_F(ConfigTest, ExcerptDoesNotSplitAUtf8Character) {
    std::string line = "a";
    for (int index = 0; index < 300; ++index) {
        line += "\xC3\xA9";
    }
    from_file("[*]\n" + line + "\n");
    ASSERT_EQ(sink.lines.size(), 1u);
    const std::string& message = sink.lines[0].message;
    const std::size_t marker = message.find("...");
    ASSERT_NE(marker, std::string::npos);
    EXPECT_EQ(static_cast<unsigned char>(message[marker - 1]), 0xA9);
}

TEST_F(ConfigTest, InvalidIntegerDefaultIsReportedNotReadAsZeroSilently) {
    constexpr std::array<OptionSpec, 1> broken = {{
        {"count", "not a number", "", OptionType::integer, {}},
    }};
    const Config config = Config::resolve({"game.exe", std::nullopt, std::nullopt}, broken);
    EXPECT_EQ(config.get_int("count"), 0);
    ASSERT_EQ(sink.lines.size(), 1u);
    EXPECT_EQ(sink.lines[0].level, log::Level::error);
    EXPECT_EQ(sink.lines[0].message,
              "config: option \"count\" holds \"not a number\", not an integer");
}

TEST_F(ConfigTest, EveryTestRegistryDefaultIsValid) {
    for (const OptionSpec& option : test_registry) {
        const Sources sources{"x", std::nullopt,
                              std::string(option.key) + "=" + std::string(option.default_value)};
        Config::resolve(sources, test_registry);
    }
    EXPECT_TRUE(sink.lines.empty());
}

TEST(ConfigRegistry, EveryDefaultIsValidForItsType) {
    std::set<std::string> keys;
    for (const OptionSpec& option : options()) {
        EXPECT_TRUE(keys.insert(std::string(option.key)).second) << option.key;
        EXPECT_FALSE(option.description.empty()) << option.key;
        EXPECT_EQ(option.type == OptionType::choice, !option.choices.empty()) << option.key;
        log::testing_support::CapturingSink sink;
        log::set_sink(&sink);
        const Sources sources{"x", std::nullopt,
                              std::string(option.key) + "=" + std::string(option.default_value)};
        Config::resolve(sources);
        log::set_sink(nullptr);
        EXPECT_TRUE(sink.lines.empty()) << option.key;
    }
}

} // namespace
