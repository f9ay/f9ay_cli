#include <filesystem>
#include <variant>
#include "png.hpp"
#include "jpeg.hpp"
#include "bmp.hpp"
#include <ranges>
#include <print>
#include <chrono>

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
                if (i + 1 >= argc || std::string_view(argv[i + 1]).starts_with("-")) {
                    option[arg] = "";
                    continue;
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

static std::variant<f9ay::Bmp, f9ay::Jpeg<>, f9ay::PNG> selectImporter(const std::byte *data) {
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
    if (parser.option.contains("h")) {
        std::println("Usage: f9ay <input file> <output file> [options]");
        std::println("Options:");
        std::println("  -i <input file>");
        std::println("  -o <output file>");
        std::println("  -benchmark");

        std::println("Jpeg options:");
        std::println("  -s <4:4:4 or 4:2:0>     setting jpeg downsampling ratio, default is 4:2:0");
        return 0;
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
        std::println("error : no input file or output file");
        return -1;
    }

    std::ifstream ifs(input_file, std::ios::binary);
    if (!ifs.is_open()) {
        std::println("error : failed to open {}", input_file.string());
        return -1;
    }

    auto input_buffer = f9ay::readFile(ifs);
    ifs.close();
    std::variant<f9ay::Bmp, f9ay::Jpeg<>, f9ay::PNG> importer;
    try {
        importer = selectImporter(input_buffer.get());
    } catch (...) {
        std::println("error : unsupported input image type ", input_file.string());
        return -1;
    }
    f9ay::Midway image_midway;
    try {
        std::visit(
            [&image_midway, &input_buffer]<typename T>(T &&imp) {
                if constexpr (requires { imp.importFromByte(input_buffer.get()); }) {
                    image_midway = imp.importFromByte(input_buffer.get());
                    input_buffer.reset();
                } else {
                    throw std::runtime_error("no implement file format");
                }
            },
            importer);
    } catch (...) {
        std::println("error : no implement file format");
        return -1;
    }

    auto output_split = output_file.string()
                        | std::views::all
                        | std::views::split('.')
                        | std::ranges::to<std::vector>();
    auto output_format = std::string_view(output_split.back());
    std::variant<f9ay::Bmp, f9ay::Jpeg<f9ay::Jpeg_sampling::ds_4_2_0>, f9ay::Jpeg<f9ay::Jpeg_sampling::ds_4_4_4>, f9ay::PNG> exporter;
    if (output_format == "bmp") {
        exporter = f9ay::Bmp();
    }
    else if (output_format == "jpg" || output_format == "jpeg") {
        if (parser.option.contains("s") && parser.option["s"] == "4:4:4") {
            exporter = f9ay::Jpeg<f9ay::Jpeg_sampling::ds_4_4_4>();
        }
        else if (parser.option.contains("s") && parser.option["s"] != "4:2:0") {
            std::println("error : Illegal sampling value {}", parser.option["s"]);
            return -1;
        }
        else {
            exporter = f9ay::Jpeg<f9ay::Jpeg_sampling::ds_4_2_0>();
        }
    }
    else if (output_format == "png") {
        exporter = f9ay::PNG();
    }
    else {
        std::println("error : unsupported output format {}", output_format);
        return -1;
    }
    std::unique_ptr<std::byte[]> buffer;
    size_t size;
    std::chrono::time_point<std::chrono::high_resolution_clock> start, end;
    bool enable_benchmark = false;
    if (parser.option.contains("benchmark")) {
        enable_benchmark = true;
    }
    std::visit(
        [&buffer, &size, &start, &end, enable_benchmark](auto &&imageMtx, auto &&expo) {
            if (enable_benchmark) {
                start = std::chrono::high_resolution_clock::now();
            }
            std::tie(buffer, size) = expo.exportToByte(imageMtx);
            if (enable_benchmark) {
                end = std::chrono::high_resolution_clock::now();
            }
        },
        image_midway, exporter);

    std::ofstream out(output_file, std::ios::binary);
    if (!out.is_open()) {
        std::println("error : failed to open {}", output_file.string());
        return -1;
    }
    out.write(reinterpret_cast<const char*>(buffer.get()), size);
    std::println("done");
    if (enable_benchmark) {
        std::println("{}", std::chrono::duration_cast<std::chrono::milliseconds>(end - start));
    }
}