#include <filesystem>
#include <variant>
#include "png.hpp"
#include "jpeg.hpp"
#include "bmp.hpp"
#include <ranges>
#include <print>

using std::operator ""s;

class Parser {
public:
    void parse(int argc, char** argv) {
        for (int i = 1; i < argc; i++ ) {
            if (std::string_view(argv[i]).starts_with("-")) {
                auto arg = std::string_view(argv[i])
                    | std::views::drop_while([](auto c) {
                        return c == '-';
                    })
                    | std::ranges::to<std::string>();
                if (i + 1 >= argc) {
                    std::println("Missing argument for option '{}'", arg);
                    std::println("Error splitting the argument list: Invalid argument");
                    throw std::invalid_argument("invalid argument");
                }
                option[arg] = argv[i + 1];
                i++;
            }
            else {
                args.push_back(argv[i]);
            }
        }
    }
    std::string next_arg() {
        if (args.size() == args_index) {
            return "";
        }
        return args[args_index++];
    }
    std::map<std::string, std::string> option;
    std::vector<std::string> args;
    int args_index = 0;
};

static std::variant<f9ay::Bmp, f9ay::Jpeg, f9ay::PNG> selectImporter(const std::byte *data) {
    // buffer may overflow but I don't care
    if (data[0] == std::byte{0x89} && data[1] == std::byte{0x50} && data[2] == std::byte{0x4E} &&
        data[3] == std::byte{0x47}) {
        return f9ay::PNG();
    }
    if (data[0] == std::byte{0xFFu} && data[1] == std::byte{0xD8u}) {
        return f9ay::Jpeg();
    }
    if (data[0] == std::byte{0x42u} && data[1] == std::byte{0x4Du}) {
        return f9ay::Bmp();
    }
    throw std::runtime_error("Unsupported image format");
}

int main(int argc, char** argv) {
    Parser parser;
    try {
        parser.parse(argc, argv);
    } catch (...) {
        return -1;
    }
    std::filesystem::path input_file;
    if (parser.option.contains("i")) {
        input_file = parser.option["i"];
    }
    else {
        input_file = parser.next_arg();
    }
    std::filesystem::path output_file;
    if (parser.option.contains("o")) {
        output_file = parser.option["o"];
    }
    else {
        output_file = parser.next_arg();
    }

    if (input_file == "" || output_file == "") {
        std::println("no input file or output file");
        return -1;
    }

    std::ifstream ifs(input_file, std::ios::binary);
    if (!ifs.is_open()) {
        std::println("failed to open {}", input_file.string());
        return -1;
    }

    const auto input_buffer = f9ay::readFile(ifs);
    ifs.close();
    auto importer = selectImporter(input_buffer.get());
    f9ay::Midway image_midway;
    try {
        std::visit(
            [&image_midway, &input_buffer]<typename T>(T &&imp) {
                if constexpr (requires { imp.importFromByte(input_buffer.get()); }) {
                    image_midway = imp.importFromByte(input_buffer.get());
                } else {
                    throw std::runtime_error("no implement file format");
                }
            },
            importer);
    }catch (...) {
        std::println("no implement file format");
        return -1;
    }

    auto output_split = output_file.string()
                        | std::views::all
                        | std::views::split('.')
                        | std::ranges::to<std::vector>();
    auto output_format = std::string_view(output_split.back());
    std::variant<f9ay::Bmp, f9ay::Jpeg, f9ay::PNG> exporter;
    if (output_format == "bmp") {
        exporter = f9ay::Bmp();
    }
    else if (output_format == "jpg" || output_format == "jpeg") {
        exporter = f9ay::Jpeg();
    }
    else if (output_format == "png") {
        exporter = f9ay::PNG();
    }
    else {
        std::println("unsupported output format {}", output_format);
        return -1;
    }
    std::unique_ptr<std::byte[]> buffer;
    size_t size;
    std::visit(
        [&buffer, &size](auto &&imageMtx, auto &&exp) {
            std::tie(buffer, size) = exp.exportToByte(imageMtx);
        },
        image_midway, exporter);

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::println("failed to open {}", output_file.string());
        return -1;
    }
    out.write(reinterpret_cast<const char*>(buffer.get()), size);
    std::println("done");
}