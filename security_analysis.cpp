#include <algorithm>
#include <climits>
#include "security_analysis.h"

using namespace std;

// ---------------------------------------------------------------------------------------
// Utilidades
// ---------------------------------------------------------------------------------------

void SecurityAnalyzer::warn(int line, int col, const string& msg) {
    string key = to_string(line) + ":" + to_string(col) + ":" + msg;
    if (!seen.insert(key).second) return;
    findings.push_back({"seguridad", msg, line, col});
}

bool SecurityAnalyzer::isLocal(const IdExp* id) {
    return id->idKind == ID_VAR && id->var.storage == Storage::LOCAL && !id->isRefVar;
}

bool SecurityAnalyzer::isNullLiteral(Exp* e) {
    if (e->nodeKind() == NodeKind::NULLPTR_EXP) return true;
    return e->nodeKind() == NodeKind::NUMBER_EXP && static_cast<NumberExp*>(e)->value == 0;
}

// Variables que no son nulas cuando "cond" vale whenTrue (if (p), p != nullptr, !p, a && b ...)
void SecurityAnalyzer::guard(Exp* cond, bool whenTrue, vector<int>& nonNull) {
    if (!cond) return;
    auto ptrSlot = [&](Exp* x, int& s) {
        if (x->nodeKind() != NodeKind::ID_EXP) return false;
        IdExp* id = static_cast<IdExp*>(x);
        if (!isLocal(id) || id->inferredType.ptrLevel == 0) return false;
        s = id->var.offset;
        return true;
    };
    int s;
    if (ptrSlot(cond, s)) {
        if (whenTrue) nonNull.push_back(s);
        return;
    }
    if (cond->nodeKind() == NodeKind::UNARY_EXP) {
        UnaryExp* u = static_cast<UnaryExp*>(cond);
        if (u->op == NOT_UNARY) guard(u->exp, !whenTrue, nonNull);
        return;
    }
    if (cond->nodeKind() != NodeKind::BINARY_EXP) return;
    BinaryExp* b = static_cast<BinaryExp*>(cond);
    if (b->op == AND_OP && whenTrue) {
        guard(b->left, true, nonNull);
        guard(b->right, true, nonNull);
    } else if (b->op == OR_OP && !whenTrue) {
        guard(b->left, false, nonNull);
        guard(b->right, false, nonNull);
    } else if (b->op == NE_OP || b->op == EQ_OP) {
        Exp* v = nullptr;
        if (isNullLiteral(b->right) && ptrSlot(b->left, s)) v = b->left;
        else if (isNullLiteral(b->left) && ptrSlot(b->right, s)) v = b->right;
        if (v && ((b->op == NE_OP) == whenTrue)) nonNull.push_back(s);
    }
}

void SecurityAnalyzer::assumeNonNull(const vector<int>& slots) {
    for (int s : slots) {
        auto it = cur.facts.find(s);
        if (it != cur.facts.end() && it->second.kind == F_NULL) cur.facts.erase(it);
    }
}

SecurityAnalyzer::State SecurityAnalyzer::join(const State& a, const State& b) {
    if (a.dead) return b;
    if (b.dead) return a;
    State r;
    for (int s : a.assigned) {
        if (b.assigned.count(s)) r.assigned.insert(s);
    }
    for (const auto& kv : a.facts) {
        auto it = b.facts.find(kv.first);
        if (it != b.facts.end() && it->second.kind == kv.second.kind && it->second.depth == kv.second.depth) {
            r.facts[kv.first] = kv.second;
        }
    }
    return r;
}

void SecurityAnalyzer::killFacts(State& s) {
    s.facts.clear();
}

// ---------------------------------------------------------------------------------------
// Evaluacion de constantes enteras (con deteccion de desbordamiento)
// ---------------------------------------------------------------------------------------

bool SecurityAnalyzer::constInt(Exp* e, long long& v, bool report) {
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
            v = static_cast<NumberExp*>(e)->value;
            return v >= INT_MIN && v <= INT_MAX;
        case NodeKind::CHAR_EXP:
            v = static_cast<CharExp*>(e)->value;
            return true;
        case NodeKind::BOOL_EXP:
            v = static_cast<BoolExp*>(e)->value ? 1 : 0;
            return true;
        case NodeKind::ID_EXP: {
            IdExp* id = static_cast<IdExp*>(e);
            if (id->idKind == ID_VAR && id->var.hasConstVal && id->inferredType.ptrLevel == 0) {
                v = id->var.constVal;
                return true;
            }
            return false;
        }
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            long long a;
            if (u->op != MINUS_UNARY && u->op != PLUS_UNARY && u->op != BIT_NOT_UNARY && u->op != NOT_UNARY) return false;
            if (!constInt(u->exp, a, report)) return false;
            switch (u->op) {
                case MINUS_UNARY:
                    v = -a;
                    if (v > INT_MAX || v < INT_MIN) {
                        if (report) warn(e->line, e->col, "Desbordamiento en expresión constante: -(" + to_string(a) + ") excede el rango de int");
                        return false;
                    }
                    return true;
                case PLUS_UNARY: v = a; return true;
                case BIT_NOT_UNARY: v = ~a; return true;
                default: v = (a == 0); return true;
            }
        }
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            const Type& ot = b->operandType;
            if (b->mode != BM_NORMAL || ot.ptrLevel != 0 || !ot.arrayDims.empty()) return false;
            if (!(ot.kind == Type::INT || ot.kind == Type::BOOL || ot.kind == Type::CHAR)) return false;
            long long a, c;
            if (!constInt(b->left, a, report) || !constInt(b->right, c, report)) return false;
            long long r = 0;
            const char* sym = "?";
            switch (b->op) {
                case PLUS_OP: r = a + c; sym = "+"; break;
                case MINUS_OP: r = a - c; sym = "-"; break;
                case MUL_OP: r = a * c; sym = "*"; break;
                case DIV_OP:
                case MOD_OP:
                    if (c == 0) {
                        if (report) warn(e->line, e->col, "División entre cero en expresión constante");
                        return false;
                    }
                    if (a == INT_MIN && c == -1) {
                        if (report) warn(e->line, e->col, "Desbordamiento en expresión constante: INT_MIN / -1");
                        return false;
                    }
                    v = (b->op == DIV_OP) ? a / c : a % c;
                    return true;
                case BIT_AND_OP: v = a & c; return true;
                case BIT_OR_OP: v = a | c; return true;
                case BIT_XOR_OP: v = a ^ c; return true;
                case LSHIFT_OP:
                    if (c < 0 || c > 31) return false;
                    r = a << c;
                    sym = "<<";
                    break;
                case RSHIFT_OP:
                    if (c < 0 || c > 31) return false;
                    v = a >> c;
                    return true;
                case EQ_OP: v = (a == c); return true;
                case NE_OP: v = (a != c); return true;
                case LT_OP: v = (a < c); return true;
                case LE_OP: v = (a <= c); return true;
                case GT_OP: v = (a > c); return true;
                case GE_OP: v = (a >= c); return true;
                case AND_OP: v = (a != 0 && c != 0); return true;
                case OR_OP: v = (a != 0 || c != 0); return true;
            }
            if (r > INT_MAX || r < INT_MIN) {
                if (report) {
                    warn(e->line, e->col, "Desbordamiento en expresión constante: " + to_string(a) + " " + sym + " " + to_string(c) +
                                              " = " + to_string(r) + " excede el rango de int");
                }
                return false;
            }
            v = r;
            return true;
        }
        default:
            return false;
    }
}

// ---------------------------------------------------------------------------------------
// Retorno en todos los caminos
// ---------------------------------------------------------------------------------------

bool SecurityAnalyzer::hasBreak(ASTNode* n) {
    if (!n) return false;
    switch (n->nodeKind()) {
        case NodeKind::BREAK_STMT: return true;
        case NodeKind::BLOCK_STMT:
            for (ASTNode* i : static_cast<BlockStmt*>(n)->items) {
                if (hasBreak(i)) return true;
            }
            return false;
        case NodeKind::IF_STMT: {
            IfStmt* s = static_cast<IfStmt*>(n);
            return hasBreak(s->thenStmt) || hasBreak(s->elseStmt);
        }
        default: return false;   // un break dentro de un bucle interno no sale de este
    }
}

static bool constTrue(SecurityAnalyzer* a, Exp* c, bool (SecurityAnalyzer::*fn)(Exp*, long long&, bool)) {
    if (!c) return true;
    long long v;
    return (a->*fn)(c, v, false) && v != 0;
}

bool SecurityAnalyzer::alwaysReturns(ASTNode* n) {
    if (!n) return false;
    switch (n->nodeKind()) {
        case NodeKind::RETURN_STMT: return true;
        case NodeKind::BLOCK_STMT:
            for (ASTNode* i : static_cast<BlockStmt*>(n)->items) {
                if (alwaysReturns(i)) return true;
            }
            return false;
        case NodeKind::IF_STMT: {
            IfStmt* s = static_cast<IfStmt*>(n);
            return s->elseStmt && alwaysReturns(s->thenStmt) && alwaysReturns(s->elseStmt);
        }
        case NodeKind::WHILE_STMT: {
            WhileStmt* s = static_cast<WhileStmt*>(n);
            return constTrue(this, s->condition, &SecurityAnalyzer::constInt) && !hasBreak(s->body);
        }
        case NodeKind::FOR_STMT: {
            ForStmt* s = static_cast<ForStmt*>(n);
            return constTrue(this, s->condition, &SecurityAnalyzer::constInt) && !hasBreak(s->body);
        }
        case NodeKind::DO_WHILE_STMT: {
            DoWhileStmt* s = static_cast<DoWhileStmt*>(n);
            if (hasBreak(s->body)) return false;
            return alwaysReturns(s->body) || constTrue(this, s->condition, &SecurityAnalyzer::constInt);
        }
        default: return false;
    }
}

// ---------------------------------------------------------------------------------------
// Asignaciones a una variable (para bucles for)
// ---------------------------------------------------------------------------------------

bool SecurityAnalyzer::expAssignsSlot(Exp* e, int slot) {
    if (!e) return false;
    auto isSlot = [&](Exp* x) {
        if (x->nodeKind() != NodeKind::ID_EXP) return false;
        IdExp* id = static_cast<IdExp*>(x);
        return id->idKind == ID_VAR && id->var.storage == Storage::LOCAL && id->var.offset == slot;
    };
    switch (e->nodeKind()) {
        case NodeKind::ASSIGN_EXP: {
            AssignExp* a = static_cast<AssignExp*>(e);
            return isSlot(a->target) || expAssignsSlot(a->target, slot) || expAssignsSlot(a->value, slot);
        }
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            if ((u->op == PRE_INC_UNARY || u->op == PRE_DEC_UNARY || u->op == ADDR_UNARY) && isSlot(u->exp)) return true;
            return expAssignsSlot(u->exp, slot);
        }
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            if (b->mode == BM_CIN && isSlot(b->right)) return true;
            return expAssignsSlot(b->left, slot) || expAssignsSlot(b->right, slot);
        }
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* p = static_cast<PostfixExp*>(e);
            if ((p->kind == POST_INC || p->kind == POST_DEC) && isSlot(p->base)) return true;
            if (expAssignsSlot(p->base, slot) || expAssignsSlot(p->indexExp, slot)) return true;
            for (size_t i = 0; i < p->args.size(); i++) {
                // pasar la variable a una referencia permite modificarla
                if (isSlot(p->args[i]) && i < p->argInfo.size() && p->argInfo[i].mode == AM_REF) return true;
                if (expAssignsSlot(p->args[i], slot)) return true;
            }
            return false;
        }
        case NodeKind::NEW_EXP: {
            NewExp* n = static_cast<NewExp*>(e);
            for (Exp* a : n->ctorArgs) {
                if (expAssignsSlot(a, slot)) return true;
            }
            return false;
        }
        case NodeKind::DELETE_EXP: return expAssignsSlot(static_cast<DeleteExp*>(e)->exp, slot);
        default: return false;
    }
}

bool SecurityAnalyzer::assignsSlot(ASTNode* n, int slot) {
    if (!n) return false;
    switch (n->nodeKind()) {
        case NodeKind::BLOCK_STMT:
            for (ASTNode* i : static_cast<BlockStmt*>(n)->items) {
                if (assignsSlot(i, slot)) return true;
            }
            return false;
        case NodeKind::EXPR_STMT: return expAssignsSlot(static_cast<ExprStmt*>(n)->expr, slot);
        case NodeKind::IF_STMT: {
            IfStmt* s = static_cast<IfStmt*>(n);
            return expAssignsSlot(s->condition, slot) || assignsSlot(s->thenStmt, slot) || assignsSlot(s->elseStmt, slot);
        }
        case NodeKind::WHILE_STMT: {
            WhileStmt* s = static_cast<WhileStmt*>(n);
            return expAssignsSlot(s->condition, slot) || assignsSlot(s->body, slot);
        }
        case NodeKind::DO_WHILE_STMT: {
            DoWhileStmt* s = static_cast<DoWhileStmt*>(n);
            return expAssignsSlot(s->condition, slot) || assignsSlot(s->body, slot);
        }
        case NodeKind::FOR_STMT: {
            ForStmt* s = static_cast<ForStmt*>(n);
            for (Exp* e : s->initExprs) {
                if (expAssignsSlot(e, slot)) return true;
            }
            for (Exp* e : s->updateExprs) {
                if (expAssignsSlot(e, slot)) return true;
            }
            if (s->initDecl) {
                for (VarInit* vi : s->initDecl->inits) {
                    if (expAssignsSlot(vi->initializer, slot)) return true;
                }
            }
            return expAssignsSlot(s->condition, slot) || assignsSlot(s->body, slot);
        }
        case NodeKind::RETURN_STMT: return expAssignsSlot(static_cast<ReturnStmt*>(n)->expr, slot);
        case NodeKind::VAR_DECL:
            for (VarInit* vi : static_cast<VarDecl*>(n)->inits) {
                if (expAssignsSlot(vi->initializer, slot)) return true;
            }
            return false;
        default: return false;
    }
}

// ---------------------------------------------------------------------------------------
// Conversiones inseguras
// ---------------------------------------------------------------------------------------

void SecurityAnalyzer::checkNarrow(const Type& to, Exp* from, int line, int col) {
    if (!from) return;
    if (to.ptrLevel != 0 || !to.arrayDims.empty()) return;
    const Type& ft = from->inferredType;
    if (ft.ptrLevel != 0 || !ft.arrayDims.empty()) return;
    bool toInt = (to.kind == Type::INT || to.kind == Type::CHAR);
    bool fromReal = (ft.kind == Type::DOUBLE || ft.kind == Type::FLOAT);
    if (toInt && fromReal) {
        warn(line, col, "Conversión insegura de '" + ft.toString() + "' a '" + to.toString() + "': se pierde la parte decimal");
        return;
    }
    if (to.kind == Type::FLOAT && ft.kind == Type::DOUBLE) {
        warn(line, col, "Conversión insegura de 'double' a 'float': se pierde precisión");
        return;
    }
    if (to.kind == Type::CHAR && ft.kind == Type::INT) {
        long long v;
        if (constInt(from, v, false)) {
            if (v < -128 || v > 127) {
                warn(line, col, "Conversión insegura de int a char: el valor constante " + to_string(v) + " no cabe en un char");
            }
        } else if (from->nodeKind() != NodeKind::BINARY_EXP && from->nodeKind() != NodeKind::UNARY_EXP) {
            // la aritmetica con char (c + 1) es un modismo habitual: no se avisa
            warn(line, col, "Conversión insegura de 'int' a 'char': puede perder información");
        }
    }
}

// ---------------------------------------------------------------------------------------
// Expresiones
// ---------------------------------------------------------------------------------------

void SecurityAnalyzer::readLocal(IdExp* id) {
    if (!isLocal(id)) return;
    int s = id->var.offset;
    if (tracked.count(s) && !cur.assigned.count(s) && !cur.dead) {
        if (reportedUninit.insert(s).second) {
            warn(id->line, id->col, "La variable '" + id->getFullName() + "' se usa sin haber sido inicializada");
        }
        cur.assigned.insert(s);
    }
}

void SecurityAnalyzer::derefCheck(Exp* p, int line, int col, const char* what) {
    if (cur.dead) return;
    if (p->nodeKind() == NodeKind::NULLPTR_EXP) {
        warn(line, col, string("Desreferencia de puntero nulo evidente (") + what + ")");
        return;
    }
    if (p->nodeKind() != NodeKind::ID_EXP) return;
    IdExp* id = static_cast<IdExp*>(p);
    if (!isLocal(id)) return;
    auto it = cur.facts.find(id->var.offset);
    if (it == cur.facts.end()) return;
    string name = id->getFullName();
    switch (it->second.kind) {
        case F_NULL:
            warn(line, col, "Desreferencia de puntero nulo evidente: '" + name + "' vale nullptr (" + what + ")");
            break;
        case F_FREED:
            warn(line, col, "Uso después de liberar: '" + name + "' ya fue liberado con delete (" + what + ")");
            break;
        case F_DANGLING:
            warn(line, col, "Puntero colgante: '" + name + "' apunta a la variable local '" + it->second.var +
                                "', cuyo bloque ya terminó (" + what + ")");
            break;
        default: break;
    }
}

void SecurityAnalyzer::exprList(const vector<Exp*>& v) {
    for (Exp* e : v) expr(e);
}

// Lado izquierdo de una asignacion: no se lee la variable, pero sus subexpresiones si
void SecurityAnalyzer::lvalue(Exp* e) {
    switch (e->nodeKind()) {
        case NodeKind::ID_EXP: return;
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            if (u->op == DEREF_UNARY) {
                derefCheck(u->exp, u->line, u->col, "*p");
                expr(u->exp);
            } else {
                expr(e);
            }
            return;
        }
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* p = static_cast<PostfixExp*>(e);
            if (p->kind == ARRAY_ACCESS) {
                checkIndex(p);
                if (p->base->inferredType.arrayDims.empty()) derefCheck(p->base, p->line, p->col, "p[i]");
                expr(p->base);
                expr(p->indexExp);
            } else if (p->kind == ARROW_ACCESS) {
                derefCheck(p->base, p->line, p->col, "p->campo");
                expr(p->base);
            } else if (p->kind == DOT_ACCESS) {
                lvalue(p->base);
            } else {
                expr(e);
            }
            return;
        }
        default: expr(e); return;
    }
}

void SecurityAnalyzer::assignTo(Exp* target, Exp* value, int line, int col) {
    if (value) expr(value);
    checkNarrow(target->inferredType.withoutRef(), value, line, col);
    if (target->nodeKind() == NodeKind::ID_EXP) {
        IdExp* id = static_cast<IdExp*>(target);
        if (isLocal(id)) {
            int s = id->var.offset;
            cur.assigned.insert(s);
            cur.facts.erase(s);
            if (value && id->inferredType.ptrLevel > 0) {
                if (isNullLiteral(value)) {
                    cur.facts[s] = Fact{F_NULL, 0, ""};
                } else if (value->nodeKind() == NodeKind::UNARY_EXP) {
                    UnaryExp* u = static_cast<UnaryExp*>(value);
                    if (u->op == ADDR_UNARY && u->exp->nodeKind() == NodeKind::ID_EXP) {
                        IdExp* x = static_cast<IdExp*>(u->exp);
                        if (isLocal(x) && declDepth.count(x->var.offset) && declDepth.count(s) &&
                            declDepth[x->var.offset] > declDepth[s]) {
                            cur.facts[s] = Fact{F_ADDR_LOCAL, declDepth[x->var.offset], x->getFullName()};
                        }
                    }
                }
            }
        }
        return;
    }
    lvalue(target);
}

void SecurityAnalyzer::checkIndex(PostfixExp* pf) {
    const Type& bt = pf->base->inferredType;
    if (bt.arrayDims.empty()) return;
    long long dim = bt.arrayDims[0];
    long long v;
    if (constInt(pf->indexExp, v, false)) {
        if (v < 0 || v >= dim) {
            warn(pf->line, pf->col, "Índice constante " + to_string(v) + " fuera de rango: el arreglo tiene " + to_string(dim) +
                                        " elemento(s), válidos 0.." + to_string(dim - 1));
        }
        return;
    }
    int slot;
    long long off;
    if (linearInLoopVar(pf->indexExp, slot, off)) {
        auto it = loopRange.find(slot);
        if (it != loopRange.end()) {
            long long lo = it->second.first + off, hi = it->second.second + off;
            if (hi >= dim || lo < 0) {
                string name = slotName.count(slot) ? slotName[slot] : "i";
                warn(pf->line, pf->col, "El índice (basado en '" + name + "') llega a " + to_string(hi >= dim ? hi : lo) +
                                            " pero el arreglo tiene " + to_string(dim) + " elemento(s): acceso fuera de rango");
            }
        }
    }
}

bool SecurityAnalyzer::linearInLoopVar(Exp* e, int& slot, long long& off) {
    if (e->nodeKind() == NodeKind::ID_EXP) {
        IdExp* id = static_cast<IdExp*>(e);
        if (!isLocal(id)) return false;
        slot = id->var.offset;
        off = 0;
        return true;
    }
    if (e->nodeKind() == NodeKind::BINARY_EXP) {
        BinaryExp* b = static_cast<BinaryExp*>(e);
        long long c;
        if ((b->op == PLUS_OP || b->op == MINUS_OP) && b->mode == BM_NORMAL && constInt(b->right, c, false) &&
            linearInLoopVar(b->left, slot, off)) {
            off += (b->op == PLUS_OP) ? c : -c;
            return true;
        }
    }
    return false;
}

void SecurityAnalyzer::expr(Exp* e) {
    if (!e) return;
    switch (e->nodeKind()) {
        case NodeKind::NUMBER_EXP:
        case NodeKind::REAL_EXP:
        case NodeKind::BOOL_EXP:
        case NodeKind::CHAR_EXP:
        case NodeKind::STRING_EXP:
        case NodeKind::NULLPTR_EXP:
            return;
        case NodeKind::ID_EXP:
            readLocal(static_cast<IdExp*>(e));
            return;
        case NodeKind::ASSIGN_EXP: {
            AssignExp* a = static_cast<AssignExp*>(e);
            assignTo(a->target, a->value, a->line, a->col);
            return;
        }
        case NodeKind::UNARY_EXP: {
            UnaryExp* u = static_cast<UnaryExp*>(e);
            switch (u->op) {
                case DEREF_UNARY:
                    derefCheck(u->exp, u->line, u->col, "*p");
                    expr(u->exp);
                    break;
                case ADDR_UNARY:
                    // tomar la direccion permite escribir la variable mas tarde
                    if (u->exp->nodeKind() == NodeKind::ID_EXP) {
                        IdExp* id = static_cast<IdExp*>(u->exp);
                        if (isLocal(id)) {
                            cur.assigned.insert(id->var.offset);
                            cur.facts.erase(id->var.offset);
                        }
                    } else {
                        expr(u->exp);
                    }
                    break;
                case PRE_INC_UNARY:
                case PRE_DEC_UNARY:
                    expr(u->exp);
                    break;
                default:
                    expr(u->exp);
                    break;
            }
            long long v;
            if (u->op == MINUS_UNARY) constInt(e, v, true);
            return;
        }
        case NodeKind::BINARY_EXP: {
            BinaryExp* b = static_cast<BinaryExp*>(e);
            if (b->mode == BM_CIN) {
                expr(b->left);
                if (b->right->nodeKind() == NodeKind::ID_EXP && isLocal(static_cast<IdExp*>(b->right))) {
                    int s = static_cast<IdExp*>(b->right)->var.offset;
                    cur.assigned.insert(s);
                    cur.facts.erase(s);
                } else {
                    lvalue(b->right);
                }
                return;
            }
            expr(b->left);
            if (b->op == AND_OP || b->op == OR_OP) {
                State saved = cur;
                vector<int> nn;
                guard(b->left, b->op == AND_OP, nn);
                assumeNonNull(nn);
                expr(b->right);
                cur = join(saved, cur);
            } else {
                expr(b->right);
            }
            if (b->mode == BM_NORMAL) {
                long long a, c;
                if (constInt(b->left, a, false) && constInt(b->right, c, false)) {
                    long long v;
                    constInt(e, v, true);
                } else if ((b->op == DIV_OP || b->op == MOD_OP) && b->operandType.kind != Type::DOUBLE &&
                           b->operandType.kind != Type::FLOAT && constInt(b->right, c, false) && c == 0) {
                    warn(e->line, e->col, "División entre cero: el divisor es la constante 0");
                }
            }
            return;
        }
        case NodeKind::POSTFIX_EXP: {
            PostfixExp* p = static_cast<PostfixExp*>(e);
            switch (p->kind) {
                case POST_INC:
                case POST_DEC:
                    expr(p->base);
                    return;
                case ARRAY_ACCESS:
                    checkIndex(p);
                    if (p->base->inferredType.arrayDims.empty() && !p->base->inferredType.isString()) {
                        derefCheck(p->base, p->line, p->col, "p[i]");
                    }
                    expr(p->base);
                    expr(p->indexExp);
                    return;
                case DOT_ACCESS:
                    expr(p->base);
                    return;
                case ARROW_ACCESS:
                    derefCheck(p->base, p->line, p->col, "p->campo");
                    expr(p->base);
                    return;
                case CALL_ACCESS:
                    if (p->base) {
                        if (p->callKind == CALL_METHOD) {
                            // el objeto o el puntero receptor
                            if (p->base->nodeKind() == NodeKind::POSTFIX_EXP &&
                                static_cast<PostfixExp*>(p->base)->kind == ARROW_ACCESS) {
                                PostfixExp* m = static_cast<PostfixExp*>(p->base);
                                derefCheck(m->base, p->line, p->col, "p->metodo()");
                                expr(m->base);
                            } else if (p->base->nodeKind() == NodeKind::POSTFIX_EXP &&
                                       static_cast<PostfixExp*>(p->base)->kind == DOT_ACCESS) {
                                expr(static_cast<PostfixExp*>(p->base)->base);
                            } else {
                                expr(p->base);
                            }
                        } else if (p->callKind != CALL_FUNCTION && p->callKind != CALL_CTOR && p->callKind != CALL_BUILTIN &&
                                   p->callKind != CALL_METHOD_IMPLICIT) {
                            expr(p->base);
                        }
                    }
                    for (size_t i = 0; i < p->args.size(); i++) {
                        Exp* a = p->args[i];
                        bool byRef = i < p->argInfo.size() && p->argInfo[i].mode == AM_REF;
                        if (byRef && a->nodeKind() == NodeKind::ID_EXP && isLocal(static_cast<IdExp*>(a))) {
                            // una referencia no const puede inicializar la variable
                            int s = static_cast<IdExp*>(a)->var.offset;
                            cur.assigned.insert(s);
                            cur.facts.erase(s);
                        } else {
                            expr(a);
                        }
                    }
                    return;
            }
            return;
        }
        case NodeKind::NEW_EXP: {
            NewExp* n = static_cast<NewExp*>(e);
            exprList(n->arrayDims);
            exprList(n->ctorArgs);
            return;
        }
        case NodeKind::DELETE_EXP: {
            DeleteExp* d = static_cast<DeleteExp*>(e);
            expr(d->exp);
            if (d->exp->nodeKind() == NodeKind::ID_EXP && isLocal(static_cast<IdExp*>(d->exp)) && !cur.dead) {
                IdExp* id = static_cast<IdExp*>(d->exp);
                auto it = cur.facts.find(id->var.offset);
                if (it != cur.facts.end() && it->second.kind == F_FREED) {
                    warn(d->line, d->col, "Doble liberación: '" + id->getFullName() + "' ya fue liberado con delete");
                } else if (it == cur.facts.end() || it->second.kind != F_NULL) {
                    cur.facts[id->var.offset] = Fact{F_FREED, 0, ""};
                }
            }
            return;
        }
        case NodeKind::BRACED_INIT:
            exprList(static_cast<BracedInitList*>(e)->elements);
            return;
        default:
            return;
    }
}

// ---------------------------------------------------------------------------------------
// Sentencias
// ---------------------------------------------------------------------------------------

void SecurityAnalyzer::leaveScope() {
    // las variables de este bloque dejan de existir: los punteros que las apuntan quedan colgantes
    for (auto& kv : cur.facts) {
        if (kv.second.kind == F_ADDR_LOCAL && kv.second.depth >= depth) kv.second.kind = F_DANGLING;
    }
}

void SecurityAnalyzer::varDecl(VarDecl* d) {
    for (VarInit* vi : d->inits) {
        int s = vi->info.offset;
        bool local = vi->info.storage == Storage::LOCAL;
        if (local) {
            declDepth[s] = depth;
            slotName[s] = vi->name;
        }
        for (Exp* a : vi->ctorArgs) expr(a);
        if (vi->initializer) {
            expr(vi->initializer);
            checkNarrow(vi->info.type.withoutRef(), vi->initializer, vi->line, vi->col);
        }
        if (!local) continue;
        const Type& t = vi->info.type;
        bool scalar = t.arrayDims.empty() && !vi->isRef &&
                      (t.ptrLevel > 0 || t.kind == Type::INT || t.kind == Type::FLOAT || t.kind == Type::DOUBLE ||
                       t.kind == Type::BOOL || t.kind == Type::CHAR);
        cur.facts.erase(s);
        if (!vi->initializer && vi->ctorArgs.empty() && !vi->directInit && scalar) {
            tracked.insert(s);
            cur.assigned.erase(s);
        } else {
            cur.assigned.insert(s);
            if (vi->initializer && t.ptrLevel > 0 && t.arrayDims.empty()) {
                Exp* init = vi->initializer;
                if (isNullLiteral(init)) {
                    cur.facts[s] = Fact{F_NULL, 0, ""};
                } else if (init->nodeKind() == NodeKind::UNARY_EXP) {
                    UnaryExp* u = static_cast<UnaryExp*>(init);
                    if (u->op == ADDR_UNARY && u->exp->nodeKind() == NodeKind::ID_EXP) {
                        IdExp* x = static_cast<IdExp*>(u->exp);
                        if (isLocal(x) && declDepth.count(x->var.offset) && declDepth[x->var.offset] > depth) {
                            cur.facts[s] = Fact{F_ADDR_LOCAL, declDepth[x->var.offset], x->getFullName()};
                        }
                    }
                }
            }
        }
    }
}

void SecurityAnalyzer::block(BlockStmt* b) {
    depth++;
    for (ASTNode* it : b->items) stmt(it);
    leaveScope();
    depth--;
}

void SecurityAnalyzer::loopFor(ForStmt* f) {
    depth++;
    int loopSlot = -1;
    if (f->initDecl) {
        varDecl(f->initDecl);
        // for (int i = A; i < B; i++) con A y B constantes: rango del indice
        if (f->initDecl->inits.size() == 1 && f->condition && f->updateExprs.size() == 1) {
            VarInit* vi = f->initDecl->inits[0];
            long long a, bnd;
            Exp* c = f->condition;
            if (vi->initializer && constInt(vi->initializer, a, false) && c->nodeKind() == NodeKind::BINARY_EXP) {
                BinaryExp* cb = static_cast<BinaryExp*>(c);
                bool leftIsVar = cb->left->nodeKind() == NodeKind::ID_EXP &&
                                 static_cast<IdExp*>(cb->left)->var.offset == vi->info.offset &&
                                 static_cast<IdExp*>(cb->left)->idKind == ID_VAR;
                Exp* up = f->updateExprs[0];
                bool inc = false;
                if (up->nodeKind() == NodeKind::POSTFIX_EXP) {
                    PostfixExp* pu = static_cast<PostfixExp*>(up);
                    inc = pu->kind == POST_INC && pu->base->nodeKind() == NodeKind::ID_EXP &&
                          static_cast<IdExp*>(pu->base)->var.offset == vi->info.offset;
                } else if (up->nodeKind() == NodeKind::UNARY_EXP) {
                    UnaryExp* uu = static_cast<UnaryExp*>(up);
                    inc = uu->op == PRE_INC_UNARY && uu->exp->nodeKind() == NodeKind::ID_EXP &&
                          static_cast<IdExp*>(uu->exp)->var.offset == vi->info.offset;
                }
                if (leftIsVar && inc && (cb->op == LT_OP || cb->op == LE_OP) && constInt(cb->right, bnd, false) &&
                    !assignsSlot(f->body, vi->info.offset)) {
                    long long hi = (cb->op == LT_OP) ? bnd - 1 : bnd;
                    if (a <= hi) {
                        loopRange[vi->info.offset] = {a, hi};
                        loopSlot = vi->info.offset;
                    }
                }
            }
        }
    }
    for (Exp* e : f->initExprs) expr(e);
    State entry = cur;
    expr(f->condition);
    State afterCond = cur;
    bool infinite = !f->condition || constTrue(this, f->condition, &SecurityAnalyzer::constInt);

    loops.push_back(LoopCtx());
    killFacts(cur);
    stmt(f->body);
    for (Exp* e : f->updateExprs) expr(e);
    LoopCtx ctx = loops.back();
    loops.pop_back();
    if (loopSlot >= 0) loopRange.erase(loopSlot);

    State after = infinite ? State() : afterCond;
    if (infinite) {
        after.dead = true;
    }
    if (ctx.hasBreakState) after = join(after, ctx.breakState);
    killFacts(after);
    cur = after;
    leaveScope();
    depth--;
    (void)entry;
}

void SecurityAnalyzer::stmt(ASTNode* n) {
    if (!n) return;
    switch (n->nodeKind()) {
        case NodeKind::BLOCK_STMT: block(static_cast<BlockStmt*>(n)); return;
        case NodeKind::VAR_DECL: varDecl(static_cast<VarDecl*>(n)); return;
        case NodeKind::EXPR_STMT: expr(static_cast<ExprStmt*>(n)->expr); return;
        case NodeKind::IF_STMT: {
            IfStmt* s = static_cast<IfStmt*>(n);
            expr(s->condition);
            State base = cur;
            vector<int> nnThen, nnElse;
            guard(s->condition, true, nnThen);
            guard(s->condition, false, nnElse);
            assumeNonNull(nnThen);
            stmt(s->thenStmt);
            State thenState = cur;
            cur = base;
            assumeNonNull(nnElse);
            if (s->elseStmt) stmt(s->elseStmt);
            cur = join(thenState, cur);
            return;
        }
        case NodeKind::WHILE_STMT: {
            WhileStmt* s = static_cast<WhileStmt*>(n);
            expr(s->condition);
            State afterCond = cur;
            bool infinite = constTrue(this, s->condition, &SecurityAnalyzer::constInt);
            loops.push_back(LoopCtx());
            killFacts(cur);
            stmt(s->body);
            LoopCtx ctx = loops.back();
            loops.pop_back();
            State after = afterCond;
            if (infinite) after.dead = true;
            if (ctx.hasBreakState) after = join(after, ctx.breakState);
            killFacts(after);
            cur = after;
            return;
        }
        case NodeKind::DO_WHILE_STMT: {
            DoWhileStmt* s = static_cast<DoWhileStmt*>(n);
            loops.push_back(LoopCtx());
            killFacts(cur);
            stmt(s->body);
            expr(s->condition);
            LoopCtx ctx = loops.back();
            loops.pop_back();
            State after = cur;
            if (constTrue(this, s->condition, &SecurityAnalyzer::constInt)) after.dead = true;
            if (ctx.hasBreakState) after = join(after, ctx.breakState);
            killFacts(after);
            cur = after;
            return;
        }
        case NodeKind::FOR_STMT: loopFor(static_cast<ForStmt*>(n)); return;
        case NodeKind::BREAK_STMT:
            if (!loops.empty()) {
                if (!loops.back().hasBreakState) {
                    loops.back().breakState = cur;
                    loops.back().hasBreakState = true;
                } else {
                    loops.back().breakState = join(loops.back().breakState, cur);
                }
            }
            cur.dead = true;
            return;
        case NodeKind::CONTINUE_STMT: cur.dead = true; return;
        case NodeKind::RETURN_STMT: {
            ReturnStmt* r = static_cast<ReturnStmt*>(n);
            if (r->expr) {
                expr(r->expr);
                if (curFun) {
                    checkNarrow(curFun->returnType.withoutRef(), r->expr, r->line, r->col);
                    const Type& rt = curFun->returnType;
                    Exp* x = r->expr;
                    if (rt.ptrLevel > 0 && x->nodeKind() == NodeKind::UNARY_EXP) {
                        UnaryExp* u = static_cast<UnaryExp*>(x);
                        if (u->op == ADDR_UNARY && u->exp->nodeKind() == NodeKind::ID_EXP &&
                            isLocal(static_cast<IdExp*>(u->exp))) {
                            warn(r->line, r->col, "Puntero colgante: se devuelve la dirección de la variable local '" +
                                                      static_cast<IdExp*>(u->exp)->getFullName() + "'");
                        }
                    }
                    if (rt.ptrLevel > 0 && x->nodeKind() == NodeKind::ID_EXP) {
                        IdExp* id = static_cast<IdExp*>(x);
                        if (isLocal(id) && !id->inferredType.arrayDims.empty()) {
                            warn(r->line, r->col, "Puntero colgante: se devuelve el arreglo local '" + id->getFullName() + "'");
                        }
                    }
                    if (rt.isRef && x->nodeKind() == NodeKind::ID_EXP && isLocal(static_cast<IdExp*>(x))) {
                        warn(r->line, r->col, "Referencia colgante: se devuelve una referencia a la variable local '" +
                                                  static_cast<IdExp*>(x)->getFullName() + "'");
                    }
                }
            }
            cur.dead = true;
            return;
        }
        default: return;
    }
}

void SecurityAnalyzer::function(FunDecl* f) {
    if (!f->body) return;
    curFun = f;
    cur = State();
    tracked.clear();
    declDepth.clear();
    slotName.clear();
    reportedUninit.clear();
    loops.clear();
    loopRange.clear();
    depth = 0;
    for (VarInfo& p : f->layout.params) declDepth[p.offset] = 0;
    for (ASTNode* it : f->body->items) stmt(it);

    bool needsValue = f->funKind == FunKind::NORMAL && !f->returnType.isVoid() && f->name != "main";
    if (needsValue && !alwaysReturns(f->body)) {
        warn(f->line, f->col, "La función '" + f->name + "' puede terminar sin retornar un valor en todos sus caminos");
    }
    curFun = nullptr;
}

void SecurityAnalyzer::analyze(Program* prog) {
    for (ASTNode* d : prog->declarations) {
        if (d->nodeKind() == NodeKind::FUN_DECL) {
            function(static_cast<FunDecl*>(d));
        } else if (d->nodeKind() == NodeKind::RECORD_DECL) {
            RecordDecl* rd = static_cast<RecordDecl*>(d);
            for (RecordMember* m : rd->members) {
                if (m->funDecl) function(m->funDecl);
            }
        }
    }
    stable_sort(findings.begin(), findings.end(), [](const Diagnostic& a, const Diagnostic& b) {
        if (a.line != b.line) return a.line < b.line;
        return a.col < b.col;
    });
}
