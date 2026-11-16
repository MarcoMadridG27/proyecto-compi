#include <map>
#include <sstream>
#include "optimizer.h"

using namespace std;

// Optimizacion de mirilla (peephole) sobre el ensamblador generado. Mira ventanas de dos a
// cuatro instrucciones consecutivas (una etiqueta o directiva corta la ventana porque ocupa
// su propia linea) y las reemplaza por otras equivalentes y mas baratas.
namespace {

struct Ins {
    string text;      // linea completa
    string op;        // mnemonico, vacio si es etiqueta o directiva
    string a, b;      // operandos (a = fuente, b = destino en AT&T)
    int nops = 0;
};

string trim(const string& s) {
    size_t i = 0, j = s.size();
    while (i < j && (s[i] == ' ' || s[i] == '\t')) i++;
    while (j > i && (s[j - 1] == ' ' || s[j - 1] == '\t')) j--;
    return s.substr(i, j - i);
}

// Separa "op a, b" respetando los parentesis de operandos como 8(%rax,%rcx)
Ins parse(const string& line) {
    Ins in;
    in.text = line;
    if (line.empty() || line[0] != '\t') return in;
    string body = trim(line);
    if (body.empty() || body[0] == '.') return in;
    size_t sp = body.find(' ');
    in.op = sp == string::npos ? body : body.substr(0, sp);
    if (sp == string::npos) return in;
    string rest = trim(body.substr(sp + 1));
    int paren = 0;
    size_t cut = string::npos;
    for (size_t i = 0; i < rest.size(); i++) {
        if (rest[i] == '(') paren++;
        else if (rest[i] == ')') paren--;
        else if (rest[i] == ',' && paren == 0) {
            cut = i;
            break;
        }
    }
    if (cut == string::npos) {
        in.a = rest;
        in.nops = 1;
    } else {
        in.a = trim(rest.substr(0, cut));
        in.b = trim(rest.substr(cut + 1));
        in.nops = 2;
    }
    return in;
}

string mk(const string& op, const string& a, const string& b) {
    return "\t" + op + " " + a + ", " + b;
}

bool mentions(const string& operand, const string& reg) {
    return operand.find(reg) != string::npos;
}

bool isLoadOp(const string& op) {
    return op == "movl" || op == "movq" || op == "movsbl" || op == "movzbl";
}

}  // namespace

string runPeephole(const string& asmText, PassResult& res) {
    res.name = "peephole";
    vector<string> lines;
    {
        istringstream is(asmText);
        string l;
        while (getline(is, l)) lines.push_back(l);
    }
    map<string, int> counts;
    auto note = [&](const char* rule) {
        res.count++;
        counts[rule]++;
    };

    bool changed = true;
    int rounds = 0;
    while (changed && rounds++ < 8) {
        changed = false;
        vector<string> out;
        size_t i = 0;
        while (i < lines.size()) {
            Ins x = parse(lines[i]);
            Ins y = i + 1 < lines.size() ? parse(lines[i + 1]) : Ins();
            Ins z = i + 2 < lines.size() ? parse(lines[i + 2]) : Ins();
            Ins w = i + 3 < lines.size() ? parse(lines[i + 3]) : Ins();

            // R1: leaq M, %rax ; movX (%rax), %eax|%rax  =>  movX M, dst
            if (x.op == "leaq" && x.nops == 2 && x.b == "%rax" && isLoadOp(y.op) && y.nops == 2 && y.a == "(%rax)" &&
                (y.b == "%eax" || y.b == "%rax")) {
                out.push_back(mk(y.op, x.a, y.b));
                note("lea+carga fusionadas en una carga directa");
                i += 2;
                changed = true;
                continue;
            }
            // R3: pushq %rax ; movl SRC, %eax ; movl %eax, %ecx ; popq %rax  =>  movl SRC, %ecx
            if (x.op == "pushq" && x.a == "%rax" && y.op == "movl" && y.nops == 2 && y.b == "%eax" && !mentions(y.a, "%rax") && !mentions(y.a, "%rsp") &&
                !mentions(y.a, "%eax") && z.op == "movl" && z.a == "%eax" && z.b == "%ecx" && w.op == "popq" && w.a == "%rax") {
                out.push_back(mk("movl", y.a, "%ecx"));
                note("push/pop de un operando simple eliminado");
                i += 4;
                changed = true;
                continue;
            }
            // R2: pushq %rax ; popq %rax  =>  nada ; pushq %rax ; popq %rcx  =>  movq %rax, %rcx
            if (x.op == "pushq" && x.a == "%rax" && y.op == "popq") {
                if (y.a == "%rax") {
                    note("pushq/popq consecutivos eliminados");
                } else {
                    out.push_back(mk("movq", "%rax", y.a));
                    note("pushq/popq consecutivos convertidos en movq");
                }
                i += 2;
                changed = true;
                continue;
            }
            // R4: movX %reg, M ; movX M, %reg  =>  movX %reg, M
            if ((x.op == "movl" || x.op == "movq") && x.nops == 2 && y.op == x.op && y.nops == 2 && x.a[0] == '%' &&
                y.b == x.a && y.a == x.b && x.b.find('(') != string::npos) {
                out.push_back(lines[i]);
                note("recarga inmediata tras almacenar eliminada");
                i += 2;
                changed = true;
                continue;
            }
            // R5: jmp L ; L:  =>  L:
            if (x.op == "jmp" && i + 1 < lines.size() && lines[i + 1] == x.a + ":") {
                note("salto a la etiqueta siguiente eliminado");
                i += 1;
                changed = true;
                continue;
            }
            // R6: addq $0, R  /  imulq $1, R  /  movq R, R
            if ((x.op == "addq" || x.op == "addl" || x.op == "subq" || x.op == "subl") && x.a == "$0" && x.nops == 2) {
                note("suma o resta de cero eliminada");
                i += 1;
                changed = true;
                continue;
            }
            if ((x.op == "imulq" || x.op == "imull") && x.a == "$1" && x.nops == 2) {
                note("multiplicacion por uno eliminada");
                i += 1;
                changed = true;
                continue;
            }
            if (x.op == "movq" && x.nops == 2 && x.a == x.b) {
                note("movq de un registro a si mismo eliminado");
                i += 1;
                changed = true;
                continue;
            }
            out.push_back(lines[i]);
            i++;
        }
        lines.swap(out);
    }

    for (const auto& kv : counts) res.details.push_back(kv.first + ": " + to_string(kv.second));
    string joined;
    for (const string& l : lines) joined += l + "\n";
    return joined;
}
