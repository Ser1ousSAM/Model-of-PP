#include <iostream>
#include <unordered_map>
#include <string>
#include <functional>
#include <array>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <sstream>

// настройка операции очистки консоли
#ifndef CLEAR
#define CLEAR "cls"
#endif
// настройка регулировки пошагового исполнения
#ifndef DEBUG
#define DEBUG true
#endif
// настройка вывода состояния
#ifndef OUTPUT
#define OUTPUT true
#endif

// forward-declarations
class reference;
class model;

// тип исполняемой в модели операции
typedef std::function<void(model&)> command;

// тип программы, исполняемой в модели
struct program {

	typedef std::unordered_map<std::string, unsigned int> label_mapping;

	std::vector<std::string> code;              // текстовое представление программы
	std::vector<command> commands;              // функциональное представление программы (набор команд)
	label_mapping label_to_command;             // соответствие между метками и номерами команд для прыжков
	std::vector<unsigned int> command_to_line;  // соответствие между командами и номерами строк для вывода

};

unsigned int min(unsigned int x, unsigned int y) {
	return x < y ? x : y;
}

// класс модели исполнителя
class model {

	// friend-доступ для парсера позволяет создавать команды со ссылками на регистры
	friend class parser;

	// 6 регистров общего назначения
	int eax{}; // eax - регистр-ресивер арифметических операций
	int ebx{};
	int ecx{};
	int edx{};
	int eex{};
	int efx{};

	// регистр instruction pointer, указывающий номер исполняемой операции
	int ip{};

	// 4 флага общего назначения
	int af{}; // af - флаг-ресивер логических операций
	int bf{};
	int cf{};
	int df{};

	std::ofstream& out;                     // поток вывода результатов работы программы
	std::vector<std::string> output{};      // результаты работы программы
	std::array<int, 10000> memory{};        // модель RAM, хранящей данные
	std::vector<int> stack{};               // модель стека вызовов
	program const code;                     // модель RAM, хранящей инструкции

	// внутренний флаг для отслеживания прыжков
	bool jumped{};

	int rref(reference const& loc) const;   // read-доступ к данным
	int& wref(reference const& loc);        // write-доступ к данным (недоступен для константных данных)

	// команды прыжков, изменяющие ip (операторы goto, if, else)
	void jump(reference const& x) {
		ip = rref(x);
		jumped = true;
	}
	void jz(reference const& x) {
		if (af) {
			jump(x);
		}
	}
	void jnz(reference const& x) {
		if (!af) {
			jump(x);
		}
	}

	// команды для работы с метками как с функциями
	void call(reference const& x) {
		stack.push_back(ip);
		jump(x);
	}
	void ret() {
		ip = stack.back();
		stack.pop_back();
	}

	// команды для работы с памятью
	void mov(reference const& x, reference const& y) {
		wref(x) = rref(y);
	}
	void push(reference const& x) {
		stack.push_back(rref(x));
	}
	void pop(reference const& x) {
		wref(x) = stack.back();
		stack.pop_back();
	}

	// примитивные inplace арифметические операции
	void inc(reference const& x) {
		wref(x)++;
	}
	void dec(reference const& x) {
		wref(x)--;
	}
	void neg(reference const& x) {
		wref(x) = -rref(x);
	}

	// арифметические операции с ресивером eax
	void add(reference const& x, reference const& y) {
		eax = rref(x) + rref(y);
	}
	void sub(reference const& x, reference const& y) {
		eax = rref(x) - rref(y);
	}
	void mul(reference const& x, reference const& y) {
		eax = rref(x) * rref(y);
	}
	void div(reference const& x, reference const& y) {
		eax = rref(x) / rref(y);
	}
	void mod(reference const& x, reference const& y) {
		eax = rref(x) % rref(y);
	}

	// примитивные inplace логические операции
	void set(reference const& x) {
		wref(x) = 1;
	}
	void unset(reference const& x) {
		wref(x) = 0;
	}

	// логические операции с ресивером af
	void lnot(reference const& x) {
		af = !rref(x);
	}
	void land(reference const& x, reference const& y) {
		af = rref(x) && rref(y);
	}
	void lor(reference const& x, reference const& y) {
		af = rref(x) || rref(y);
	}
	void eq(reference const& x, reference const& y) {
		af = rref(x) == rref(y);
	}
	void neq(reference const& x, reference const& y) {
		af = rref(x) != rref(y);
	}
	void less(reference const& x, reference const& y) {
		af = rref(x) < rref(y);
	}
	void leq(reference const& x, reference const& y) {
		af = rref(x) <= rref(y);
	}

	// операция численного вывода
	void print(reference const& x) {
		std::stringstream ss;
		ss << rref(x);
		std::string line;
		ss >> line;
		output.push_back(line);
	}

public:

	explicit model(program& code, std::ofstream& out) : code(code), out(out) {};

	// метод вывода состояния модели
	void log() const {
		system(CLEAR);

		// вывод текущих исполняемых инструкций с указателем на ip
		if (ip < code.commands.size()) {
			unsigned int for_print = min(code.code.size(), 21), before = for_print / 2;
			unsigned int middle = code.command_to_line[ip], start = (middle >= before ? middle - before : 0);
			if (start + for_print > code.code.size()) {
				start = code.code.size() - for_print;
			}
			for (unsigned int i = start; i < start + for_print; i++) {
				std::cout << (i == middle ? "IP >" : "    ") << code.code[i] << '\n';
			}
			std::cout << '\n';
		}

		// вывод состояния RAM
		std::cout << "memory[" << eex << ":...]: ";
		for (unsigned int i = eex; i < eex + 21; i++) {
			if (i != eex) {
				std::cout << " , ";
			}
			std::cout << memory[i];
		}
		// вывод состояния стека
		std::cout << "\nstack: ";
		for (int i = 0; i < stack.size(); i++) {
			if (i != 0) {
				std::cout << " , ";
			}
			std::cout << stack[i];
		}

		// вывод значений регистров и флагов
		std::cout << '\n';
		std::cout << "\neax: " << eax;
		std::cout << "\nebx: " << ebx;
		std::cout << "\necx: " << ecx;
		std::cout << "\nedx: " << edx;
		std::cout << "\neex: " << eex;
		std::cout << "\nefx: " << efx;
		std::cout << "\naf : " << (af ? "true" : "false");
		std::cout << "\nbf : " << (bf ? "true" : "false");
		std::cout << "\ncf : " << (cf ? "true" : "false");
		std::cout << "\ndf : " << (df ? "true" : "false");
		std::cout << '\n';

		// вывод результатов работы программы
		std::cout << "\noutput:";
		for (std::string const& line : output) {
			std::cout << "\n" << line;
		}
		std::cout << std::endl;
	}

	// метод, выполняющий один шаг программы (шаг номер ip)
	void step() {
		code.commands[ip](*this);
		if (jumped) {
			jumped = false;
		}
		else {
			ip++;
		}
	}

	// метод, выполняющий всю программу целиком до ее завершения с выводом состояния после каждого шага
	void execute(std::vector<int> const& starting_memory) {
		for (unsigned int i = 0; i < starting_memory.size(); i++) {
			memory[i] = starting_memory[i];
		}

		if (OUTPUT) log();
		if (DEBUG) std::cin.get();
		while (ip < code.commands.size()) {
			step();
			if (OUTPUT) log();
			if (DEBUG) std::cin.get();
		}

		for (std::string const& line : output) {
			out << line << std::endl;
		}
	}

};

// класс ссылки на данные в модели
class reference {

	// friend-доступ для модели позволяет избавиться от лишних публичных методов
	friend class model;

	// тип ссылки
	const enum class type {
		CONSTANT,       // 42       константное значение
		MEM_CONSTANT,   // [42]     ячейка памяти
		REGPTR,         // eax      значение регистра
		MEM_REGPTR      // [eax]    ячейка памяти, адрес которой записан в регистре
	} status;

	// сама ссылка на данные
	const union ref {
		int constant;               // значение
		unsigned int mem_constant;  // адрес ячейки памяти
		int model::* regptr;         // указатель на регистр
		int model::* mem_regptr;     // указатель на регистр, хранящий адрес ячейки памяти

		explicit ref(int x) : constant(x) {}
		explicit ref(unsigned int x) : mem_constant(x) {}
		explicit ref(int model::* x) : regptr(x) {}
		explicit ref(int model::* x, int) : mem_regptr(x) {}
	} data;

	reference(type const& status, ref const& data) : status(status), data(data) {}

public:

	reference(reference const& other) = default;

	// static-методы позволяют не путаться в конструкторах с одинаковыми типами аргументов
	static reference constant(int value) {
		return reference{ type::CONSTANT, ref(value) };
	}
	static reference mem_constant(unsigned int index) {
		return reference{ type::MEM_CONSTANT, ref(index) };
	}
	static reference regptr(int model::* ptr) {
		return reference{ type::REGPTR, ref(ptr) };
	}
	static reference mem_regptr(int model::* ptr) {
		return reference{ type::MEM_REGPTR, ref(ptr, 0) };
	}

};

// реализация read-доступа к данным
int model::rref(const reference& loc) const {
	switch (loc.status) {
	case reference::type::CONSTANT:
		return loc.data.constant;
	case reference::type::MEM_CONSTANT:
		return memory[loc.data.mem_constant];
	case reference::type::REGPTR:
		return this->*loc.data.regptr;
	case reference::type::MEM_REGPTR:
		return memory[this->*loc.data.mem_regptr];
	}
}

// реализация write-доступа к данным
int& model::wref(const reference& loc) {
	switch (loc.status) {
	case reference::type::CONSTANT:
		// в константное значение нельзя писать
		throw std::logic_error("Unable to create an assignable reference to a constant value");
	case reference::type::MEM_CONSTANT:
		return memory[loc.data.mem_constant];
	case reference::type::REGPTR:
		return this->*loc.data.regptr;
	case reference::type::MEM_REGPTR:
		return memory[this->*loc.data.mem_regptr];
	}
}

// класс, транслирующий текстовое представление кода в объект класса program
class parser {

	// соответствие между именами команд от двух аргументов и соответствующими методами над моделью
	const std::unordered_map<std::string, void (model::*)(reference const&, reference const&)> bimethods{
			{"MOV",  &model::mov},      // скопировать данные
			{"ADD",  &model::add},      // сложить два числа
			{"SUB",  &model::sub},      // вычесть два числа
			{"MUL",  &model::mul},      // умножить два числа
			{"DIV",  &model::div},      // целочисленно поделить два числа
			{"MOD",  &model::mod},      // взять остаток при делении одного числа на другое
			{"AND",  &model::land},     // взять логическое "И" двух флагов
			{"OR",   &model::lor},      // взять логическое "ИЛИ" двух флагов
			{"EQ",   &model::eq},       // проверить два значения на равенство
			{"NEQ",  &model::neq},      // проверить два значения на неравенство
			{"LESS", &model::less},     // проверить, что первое значение меньше второго
			{"LEQ",  &model::leq},      // проверить, что первое значение не больше второго
	};
	// соответствие между именами команд от одного аргумента и соответствующими методами над моделью
	const std::unordered_map<std::string, void (model::*)(reference const&)> unimethods{
			{"JUMP",  &model::jump},    // сделать прыжок на указанную метку/инструкцию
			{"JZ",    &model::jz},      // сделать прыжок, если выставлен af
			{"JNZ",   &model::jnz},     // сделать прыжок, если не выставлен af
			{"PUSH",  &model::push},    // положить значение на вершину стека
			{"POP",   &model::pop},     // достать значение из вершины стека по указанному адресу
			{"CALL",  &model::call},    // сделать прыжок и запомнить ip
			{"INC",   &model::inc},     // увеличить значение на 1 (inplace)
			{"DEC",   &model::dec},     // уменьшить значение на 1 (inplace)
			{"NEG",   &model::neg},     // инвертировать значение (inplace)
			{"SET",   &model::set},     // установить флаг (inplace)
			{"UNSET", &model::unset},   // снять флаг (inplace)
			{"NOT",   &model::lnot},    // взять логическое "НЕТ" флага
			{"PRINT", &model::print},   // вывести число
	};
	// соответствие между именами команд без аргументов и соответствующими методами над моделью
	const std::unordered_map<std::string, void (model::*)()> nullmethods{
			{"RETURN", &model::ret},    // достать ip с вершины стека
	};

	// соответствие между именами регистров и флагов и соответствующими данными в модели
	const std::unordered_map<std::string, int model::*> registers{
			{"eax", &model::eax},
			{"ebx", &model::ebx},
			{"ecx", &model::ecx},
			{"edx", &model::edx},
			{"eex", &model::eex},
			{"efx", &model::efx},
			{"ip",  &model::ip},
			{"af",  &model::af},
			{"bf",  &model::bf},
			{"cf",  &model::cf},
			{"df",  &model::df},
	};

	// функция для проверки соответствия подотрезка строки заданному условию
	static bool check(std::string const& token, std::function<bool(char)> const& predicate,
		unsigned int lm = 0, unsigned int rm = 0) {
		return std::all_of(token.begin() + lm, token.end() - rm, predicate);
	}
	// функция для перевода строкового представления числа в число
	static int string_to_int(std::string const& token, unsigned int lm = 0, unsigned int rm = 0) {
		int value = 0;
		bool negative = token[lm] == '-';
		for (unsigned int i = lm + negative; i + rm < token.size(); i++) {
			value = value * 10 + (token[i] - '0');
		}
		return negative ? -value : value;
	}

	// предикаты "цифра"/"буква"
	static bool is_digit(char c) {
		return '0' <= c && c <= '9';
	}
	static bool is_letter(char c) {
		return 'a' <= c && c <= 'z' || c == '_';
	}

	// функции проверки соответствия строк определенным грамматическим конструкциям
	// метка (label:), число (42), число в скобках ([42]), слово (eax), слово в скобках ([eax])
	static bool is_label(std::string const& token) {
		return token.back() == ':' && check(token, &is_letter, 0, 1);
	}
	static bool is_number(std::string const& token) {
		return (is_digit(token.front()) || token.front() == '-') && check(token, &is_digit, 1, 0);
	}
	static bool is_number_br(std::string const& token) {
		return token.front() == '[' && token.back() == ']' && check(token, &is_digit, 1, 1);
	}
	static bool is_word(std::string const& token) {
		return check(token, &is_letter);
	}
	static bool is_word_br(std::string const& token) {
		return token.front() == '[' && token.back() == ']' && check(token, &is_letter, 1, 1);
	}

public:

	// обертка над доступом к регистрам модели по имени
	int model::* get_register(std::string const& name) const {
		if (registers.count(name)) {
			return registers.at(name);
		}
		throw std::logic_error("Unknown register name: " + name);
	}

	// метод, создающий объект класса reference по заданному слову из кода
	reference parse_argument(program::label_mapping const& labels, std::string const& token) const {
		if (is_label(token)) {
			// метка транслируется в номер следующей за ней команды
			if (labels.count(token)) {
				return reference::constant(labels.at(token));
			}
			throw std::logic_error("Unknown label: " + token);
		}
		else if (is_number(token)) {
			// число транслируется в константу
			return reference::constant(string_to_int(token));
		}
		else if (is_number_br(token)) {
			// число в скобках транслируется в ячейку памяти
			return reference::mem_constant(string_to_int(token, 1, 1));
		}
		else if (is_word(token)) {
			// слово транслируется в указатель на регистр
			return reference::regptr(get_register(token));
		}
		else if (is_word_br(token)) {
			// слово в скобках транслируется в ячейку памяти по адресу, записанному в регистре
			std::string name = token.substr(1, token.size() - 2);
			return reference::mem_regptr(get_register(name));
		}
		else {
			// другие типы данных не поддерживаются
			throw std::logic_error("Unknown operand: " + token);
		}
	}

	// метод, создающий объект класса program из входного потока с текстом программы
	program parse(std::ifstream& in) const {
		program code{};

		std::vector<std::vector<std::string>> tokens;   // слова из строк входного потока
		std::string line;                               // текущая строка
		unsigned int cmd = 0;                           // количество команд
		while (std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back(); // platform-specific
			}
			code.code.push_back(line);
			if (line.empty()) {
				continue;
			}
			if (is_label(line)) {
				// обновление соответствия между меткой и номером соответствующей инструкции
				code.label_to_command[line] = cmd;
			}
			else {
				std::stringstream ss(line);
				std::string token;
				tokens.emplace_back();
				auto& new_line = tokens.back();
				while (ss >> token) {
					new_line.push_back(token);
				}
				unsigned int argc = new_line.size() - 1; // число аргументов команды
				// проверка соответствия введенной команды и ожидаемого числа аргументов
				if ((argc == 2 && bimethods.count(new_line[0])) ||
					(argc == 1 && unimethods.count(new_line[0])) ||
					(argc == 0 && nullmethods.count(new_line[0]))) {
					cmd++;
					code.command_to_line.push_back(code.code.size() - 1);
					continue;
				}
				throw std::logic_error("Invalid command or argument count: " + line);
			}
		}

		auto parse_arg = [this, &code](std::string const& token) {
			return parse_argument(code.label_to_command, token);
		};
		for (auto& new_line : tokens) {
			if (new_line.empty() || is_label(new_line[0])) {
				continue;
			}
			unsigned int argc = new_line.size() - 1; // число аргументов команды
			// сборка исполняемой над моделью функции типа command из текстового представления команды и аргументов
			if (argc == 2) {
				reference x = parse_arg(new_line[1]), y = parse_arg(new_line[2]);
				code.commands.emplace_back([x, y, f = bimethods.at(new_line[0])](model& m) { (m.*f)(x, y); });
			}
			else if (argc == 1) {
				reference x = parse_arg(new_line[1]);
				code.commands.emplace_back([x, f = unimethods.at(new_line[0])](model& m) { (m.*f)(x); });
			}
			else if (argc == 0) {
				code.commands.emplace_back(nullmethods.at(new_line[0]));
			}
		}

		return code;
	}

};

int main() {

	std::ifstream in("parse_expression.pa", std::ios_base::in);           // входной поток с кодом
	std::ofstream out("output.txt", std::ios_base::out);    // выходной поток для вывода результата

	// example - "1+2*(22+33)*-11+(3*4)*(2+2*400/(30+7*10))*(73%63)" -> -9
	std::string memory_input;
	std::cin >> memory_input;
	std::vector<int> ascii_input;
	std::copy(memory_input.begin(), memory_input.end(), std::back_inserter(ascii_input));

	try {
		parser p;
		program code = p.parse(in);
		model m(code, out);
		// model::execute принимает на вход стартовое состояние памяти в виде std::initializer_list<int>
		m.execute(ascii_input);
	}
	catch (std::logic_error const& e) {
		std::cerr << e.what() << std::endl;
	}
	out.close();
	while (true) getchar();
	
	
}