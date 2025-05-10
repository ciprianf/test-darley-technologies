#include <iostream>
#include <iomanip>
#include <fstream>
#include <string>
#include <vector>
#include <cctype>
#include <variant>
#include <unordered_map>

// A value in the (key, value) dictionary. It is either a string
// or a number.
struct Value {

	Value() {}

	Value(std::string str) : value_(std::move(str)) {}

	Value(int64_t x) : value_(x) {}

	std::variant<int64_t, std::string> value_;
};

// Overload the << operator for Value so we can pretty print it.
std::ostream& operator<<(std::ostream& os, const Value& value) {
	if (std::holds_alternative<std::string>(value.value_)) {
		os << std::get<std::string>(value.value_);
	} else {
		os << std::get<int64_t>(value.value_);
	}
    return os;
}

// Struct to hold instrument data.
struct Instrument {
    // Key, value mapping.
	std::unordered_map<std::string, Value> m;
};

// Custom JSON parser class for the specific Binance endpoint.
class JsonParser {
private:
    const std::string& data;
    size_t pos;

    // Skip whitespace.
    void skipWhitespace() {
        while (pos < data.size() && std::isspace(data[pos])) {
            ++pos;
        }
    }

    // Expect a specific character, throw if not found.
    void expect(char c) {
        skipWhitespace();
        if (pos >= data.size() || data[pos] != c) {
            throw std::runtime_error("Expected '" + std::string(1, c) + "' at position " + std::to_string(pos));
        }
        ++pos;
    }

    // Parse a string literal (e.g., "BTC-241227-58000-C")
    std::string parseString() {
        skipWhitespace();
        if (pos >= data.size() || data[pos] != '"') {
            throw std::runtime_error("Expected string at position " + std::to_string(pos));
        }
        ++pos; // Skip opening quote.

        std::string result;
        while (pos < data.size() && data[pos] != '"') {
            result += data[pos];
            pos++;
        }

        if (pos >= data.size() || data[pos] != '"') {
            throw std::runtime_error("Unterminated string at position " + std::to_string(pos));
        }
        ++pos; // Skip closing quote
        return result;
    }

    // Parse a positive integer (e.g. 53).
    int64_t parseInt() {
    	skipWhitespace();

    	if (pos >= data.size() || !(data[pos] >= '0' && data[pos] <= '9' )) {
    		throw std::runtime_error("Expected a number at position " + std::to_string(pos));
    	}

    	int64_t number = 0;
    	while (pos < data.size() && data[pos] >= '0' && data[pos] <= '9') {
    		number = number * 10 + data[pos] - '0';
    		pos++;
    	}
    	return number;
    }

    Value parseValue() {
    	skipWhitespace();
    	if (pos >= data.size()) {
    		throw std::runtime_error("Unexpected end of content at pos " + std::to_string(pos));
    	}
    	if (data[pos] == '"') {
    		return Value(parseString());
    	} else {
    		return Value(parseInt());
    	}
    }



    // Parse a JSON object into an Instrument
    Instrument parseObject() {
        Instrument instrument;
        expect('{');

        bool first = true;
        while (pos < data.size() && data[pos] != '}') {
            if (!first) {
                expect(',');
            }
            first = false;

            // Parse key
            std::string key = parseString();
            expect(':');

            instrument.m[key] = parseValue();
            skipWhitespace();
        }

        expect('}');
        return instrument;
    }

    // Parse a JSON array of objects
    std::vector<Instrument> parseArray() {
        std::vector<Instrument> instruments;
        expect('[');

        bool first = true;
        while (pos < data.size() && data[pos] != ']') {
            if (!first) {
                expect(',');
            }
            first = false;

            instruments.push_back(parseObject());
            skipWhitespace();
        }

        expect(']');
        return instruments;
    }

public:
    JsonParser(const std::string& json_data) : data(json_data), pos(0) {}

    // Parse the JSON data. We iterate from left to right through the
    // json_data string only once, hence the complexity is linear in input
    // data size.
    //
    // To optimize for low latency, we might want to avoid allocating any extra
    // memory during the parsing (e.g. std::string).
    std::vector<Instrument> parse() {
        skipWhitespace();
        if (pos >= data.size()) {
            throw std::runtime_error("Empty JSON data");
        }
        return parseArray();
    }
};

// Read JSON data from a file
// TODO: change name to camel-case.
std::string ReadTickerData(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file '" << filename << "'" << std::endl;
        return "";
    }

    std::string json_data((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    if (json_data.empty()) {
        std::cerr << "Error: File '" << filename << "' is empty" << std::endl;
        return "";
    }

    return json_data;
}

// Print instrument statistics. For simplicity we only print
// a couple of properties.
// TODO: make it const reference. Fix indexing unordered_map.
void PrintInstrumentStats(std::vector<Instrument>& instruments) {
    if (instruments.empty()) {
        std::cerr << "No instruments to display." << std::endl;
        return;
    }

    // Print table header. We only print a couple of columns.
    std::cout << std::left
              << std::setw(20) << "Symbol"
              << std::setw(12) << "Last Price"
              << std::setw(15) << "Price Change %"
              << std::setw(10) << "Volume"
              << std::endl;
    std::cout << std::string(60, '-') << std::endl;

    // Print each instrument
    for (auto& instrument : instruments) {
        std::cout << std::left
                  << std::setw(20) << (instrument.m.find("symbol") != instrument.m.end() ? instrument.m["symbol"] : Value("N/A"))
                  << std::setw(12) << (instrument.m.find("lastPrice") != instrument.m.end() ? instrument.m["lastPrice"] : Value("N/A"))
                  << std::setw(15) << (instrument.m.find("priceChangePercent") != instrument.m.end() ? instrument.m["priceChangePercent"] : Value("N/A"))
                  << std::setw(10) << (instrument.m.find("volume") != instrument.m.end() ? instrument.m["volume"] : Value("N/A"))
                  << std::endl;
    }
}

int main() {
    // Specify the input file
    std::string filename = "ticker.json";

    // Read JSON data
    std::string json_data = ReadTickerData(filename);
    if (json_data.empty()) {
        return 1;
    }

    // Parse and print data
    try {
        JsonParser parser(json_data);
        std::vector<Instrument> instruments = parser.parse();
        PrintInstrumentStats(instruments);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}