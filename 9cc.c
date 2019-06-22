#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの型を表す値
enum {
  ND_NUM = 256, // 整数のノードの型
  TK_NUM = 256, // 整数トークン
  TK_EOF,       // 入力の終わりを表すトークン
  TK_EQ,        // '==' equal を表すトークン
  TK_NE,        // '!=' not equal を表すトークン
  TK_LE,        // '<=' less than or equal を表すトークン
  TK_GE,        // '>=' greater than or equal を表すトークン
  // 他の演算子も全て enum で定義したい衝動に駆られる
};

typedef struct Node {
  int ty;           // 演算子か ND_NUM
  struct Node *lhs; // 左辺(left-hand side)
  struct Node *rhs; // 右辺(right-hand side)
  int val;          // メンバ変数 ty が ND_NUM の場合のみ使う
} Node;

// トークンの型 Token で宣言可能になる
typedef struct {
  int ty;       // トークンの型
  int val;      // ty が TK_NUM の場合、その数値が入る
  char *input;  // トークン文字列 (エラーメッセージ用)
} Token;

// 可変長ベクタ
// Vector型を宣言できる構造体を作成
// **data はデータ本体(式)
// capacity はバッファの領域
// len はベクタに追加済みの長さ
typedef struct {
  void **data;
  int capacity;
  int len;
} Vector;

// 入力プログラム
// ポインタ変数として宣言
// これで user_input に char型(1byte)分のメモリアドレスを格納できるようになった
char *user_input;

// トークナイズした結果のトークン列はこの配列に保存する
// 100個以上のトークンは来ないものとする
Token tokens[100];

// Vector 型のバイト数分のメモリサイズを確保し
// data に void * 型を16倍した分のメモリサイズを確保する
// void * 型は64bitの場合8byteなので、 8 * 16 = 128
// 128byte分のメモリサイズを確保する
// capacity は16で固定
// len はベクタに追加済みの長さを入れていくメンバ変数になるので0が入る
Vector *new_vector() {
  Vector *vec = malloc(sizeof(Vector));
  vec->data = malloc(sizeof(void *) * 16);
  vec->capacity = 16;
  vec->len = 0;
  return vec;
}

// Vector 型の引数と、 void * 型の引数を取る
// capacity と len を比較して同じであれば、 capacity を2倍する
// 16 -> 32 -> 64 -> 128 と増えていくことになる
// data も void * 型の8byteに capacity を乗算する
// 128 -> 256 -> 512 -> 1024 と増えていく
// 比較した結果が偽であれば、 data の配列数(len)を1増やす
void vec_push(Vector *vec, void *elem) {
  if (vec->capacity == vec->len) {
    vec->capacity *= 2;
    vec->data = realloc(vec->data, sizeof(void *) * vec->capacity);
  }

  vec->data[vec->len++] = elem;
}

void expect(int line, int expected, int actual) {
  if (expected == actual) {
    return;
  }

  fprintf(stderr, "%d: %d expected, but got %d\n", line, expected, actual);
  exit(1);
}

// データ構造のユニットテスト
// __LINE__ はCプリプロセッサのマクロで、行番号を示すらしい
void runtest() {
  Vector *vec = new_vector();
  expect(__LINE__, 0, vec->len);

  for (int i = 0; i < 100; i++) {
    vec_push(vec, (void *)i);
  }

  expect(__LINE__, 100, vec->len);
  expect(__LINE__, 0, (long)vec->data[0]);
  expect(__LINE__, 50, (long)vec->data[50]);
  expect(__LINE__, 99, (long)vec->data[99]);

  printf("OK\n");
}

// エラーを報告するための関数
// printfと同じ引数を取る
void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

void error_at(char *loc, char *msg) {
  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, ""); // pos個の空白を出力
  fprintf(stderr, "^ %s\n", msg);
  exit(1);
}

// tokens[pos].ty が TK_NUM の場合に実施される処理
// トークナイズした結果をノードとして処理し、数字だよというのを定義する
// ty に ND_NUM を入れ、 val に数値そのものを入れる
// 数字は構文木の終端となるため、lhs, rhs には何も入れない
Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

// tokens[pos].ty が括弧以外の演算子の場合に実施される処理
// トークナイズした結果をノードとして処理し、演算子で非終端だよというのを定義する
// ty に演算子そのものか、 enum で定義した定数を入れる
// lhs には左辺の数字もしくは演算子(enumの定数含む)が格納されているメモリアドレスを入れる
// rhs には右辺の数字もしくは演算子(enumの定数含む)が格納されているメモリアドレスを入れる
// val は数字ではないので何も入れない
Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

// グローバル変数として int 型の pos 変数を定義
// tokens 配列のカウントアップ用に使う
int pos = 0;

// 読み込んだトークンのタイプが、token[pos].tyと同じであれば、
// pos 変数のカウントアップを実施し、真(1)を返す
// 違う場合は偽(0)を返す
int consume(int ty) {
  if (tokens[pos].ty != ty) {
    return 0;
  }

  pos++;
  return 1;
}

// term 内で expr を呼び出しているため
// それより前に型と名前を定義しておかないとエラーになる
Node *expr();

// トークンが '(' なら expr に戻って再帰実行
// expr の処理が完了したら、次に ')' があるか確認
// ない場合はエラーになる
Node *term() {
  if (consume('(')) {
    Node *node = expr();
    if (!consume(')')) {
      error_at(tokens[pos].input, "開きカッコに対する閉じカッコがありません");
    }

    return node;
  }

  // 今までの関数で処理されていなければ
  // そうでなければ数値のはず
  if (tokens[pos].ty == TK_NUM) {
    return new_node_num(tokens[pos++].val);
  }

  error_at(tokens[pos].input, "数値でも開きカッコでもないトークンです");
}

// トークンが単項プラス、単項マイナスであるか確認
// consume で確認し、+ の場合となにもない場合は term を呼び出す
// - の場合のみ、 new_node で 0 - num の形式に変換
Node *unary() {
  if (consume('+')) {
    return term(); 
  }
  if (consume('-')) {
    return new_node('-', new_node_num(0), term());
  }

  return term();
}

// トークンが *, /, % 演算子であるか確認
// 最初に unary を呼び出し、処理完了後、トークン判定に移る
// cunsume で確認し、指定したトークンがあれば、再度 unary を呼び出す
// 何もなければ 呼び出し元へ node を戻す
Node *mul() {
  Node *node = unary();

  for (;;) {
    if (consume('*')) {
      node = new_node('*', node, unary());
    } else if (consume('/')) {
      node = new_node('/', node, unary());
    } else if (consume('%')) {
      node = new_node('%', node, unary());
    } else {
      return node;
    }
  }
}

// トークンが +, - 演算子であるか確認
// 最初に mul を呼び出し、処理完了後、トークン判定に移る
// cunsume で確認し、指定したトークンがあれば、再度 mul を呼び出す
// 何もなければ 呼び出し元へ node を戻す
Node *add() {
  Node *node = mul();

  for (;;) {
    if (consume('+')) {
      node = new_node('+', node, mul());
    } else if (consume('-')) {
      node = new_node('-', node, mul());
    } else {
      return node;
    }
  }
}

// トークンが <, <=, >, >= 演算子であるか確認
// 最初に add を呼び出し、処理完了後、トークン判定に移る
// cunsume で確認し、指定したトークンがあれば、再度 add を呼び出す
// 何もなければ 呼び出し元へ node を戻す
Node *relational() {
  Node *node = add();

  for (;;) {
    if (consume(TK_LE)) {
      node = new_node(TK_LE, node, add());
    } else if (consume('<')) {
      node = new_node('<', node, add());
    } else if (consume(TK_GE)) {
      node = new_node(TK_GE, add(), node);
    } else if (consume('>')) {
      node = new_node('>', add(), node);
    } else {
      return node;
    }
  }
}

// トークンが ==, !=, 演算子であるか確認
// 最初に relational を呼び出し、処理完了後、トークン判定に移る
// cunsume で確認し、指定したトークンがあれば、再度 relational を呼び出す
// 何もなければ 呼び出し元へ node を戻す
Node *equality() {
  Node *node = relational();

  for (;;) {
    if (consume(TK_EQ)) {
      node = new_node(TK_EQ, node, relational());
    } else if (consume(TK_NE)) {
      node = new_node(TK_NE, node, relational());
    } else {
      return node;
    }
  }
}

// equality の呼び出しのみ行う
// パーサの一番最初の処理
Node *expr() {
  Node *node = equality();
}

void tokenize() {
  // char型のポインタpを宣言して、user_inputの値(メモリアドレス)を格納する
  // これで p を間接演算子で表示(*p)すると、user_inputのメモリアドレスに飛び、
  // そこから更に先の値(argv[1])を参照することができる
  char *p = user_input;

  // tokens 配列カウントアップ用のint型変数iを定義し、0を入れる
  int i = 0;

  // while(*p != NULL) と同じ意味
  // どうやら文字が入っていればそれはtrue判定となるようだ
  // 最後に終端文字がNULL(\0)となりfalseとなるため、必然的に終了する・・・みたい？
  while (*p) {
    // 空白文字をスキップ
    if (isspace(*p)) {
      p++;
      continue;
    }

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '%' || *p == '(' || *p == ')') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    // *p が = であり、次の文字も = であれば TK_EQ を tokens[i].ty に格納
    // p++ だと再度 = を読み込んでしまうため、 +2 をしている
    if (*p == '=' && *(p+1) == '=') {
      tokens[i].ty = TK_EQ;
      tokens[i].input = p;
      i++;
      p = p + 2;
      continue;
    }

    // *p が != であり、次の文字が = であれば TK_NE を tokens[i].ty に格納
    // p++ だと再度 = を読み込んでしまうため、 +2 をしている
    if (*p == '!' && *(p+1) == '=') {
      tokens[i].ty = TK_NE;
      tokens[i].input = p;
      i++;
      p = p + 2;
      continue;
    }

    // *p が <= であり、次の文字が = であれば TK_LE を tokens[i].ty に格納
    // p++ だと再度 = を読み込んでしまうため、 +2 をしている
    // *p が < のみの場合は tokens[i].ty に < をそのまま格納
    if (*p == '<' && *(p+1) == '=') {
      tokens[i].ty = TK_LE;
      tokens[i].input = p;
      i++;
      p = p + 2;
      continue;
    } else if (*p == '<') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    // *p が >= であり、次の文字が = であれば TK_GE を tokens[i].ty に格納
    // p++ だと再度 = を読み込んでしまうため、 +2 をしている
    // *p が > のみの場合は tokens[i].ty に > をそのまま格納
    if (*p == '>' && *(p+1) == '=') {
      tokens[i].ty = TK_GE;
      tokens[i].input = p;
      i++;
      p = p + 2;
      continue;
    } else if (*p == '>') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

    // *p が数値の場合は数値が続く限り tokens[i].val に格納
    if (isdigit(*p)) {
      tokens[i].ty = TK_NUM;
      // strtol - 変数p内の文字列を頭から読み、数値であればn進数で返す
      // 数値でなければ、その文字列へのポインタを&p(変数pのメモリ上のアドレスを示す)
      // に格納する
      // なので、トークナイズに成功すると、変数pには、
      // 次の文字列(+/-/etc)へのポインタを格納することになる
      // p  -> 変数pに格納されている値(読み込む数値を格納しているアドレス)を表示(都度変化)
      // printf("p  = %ld", p);
      // *p -> 変数pに格納されている値をメモリ上のアドレスと解釈し、
      //       そのアドレスの指し先となる文字列を表示(都度変化)
      // printf("*p = %ld", *p);
      // &p -> 変数pで予約したメモリアドレス自体を表示(不変)
      // printf("&p = %ld", &p);
      tokens[i].input = p;
      tokens[i].val = strtol(p, &p, 10);
      i++;
      continue;
    }

    error_at(p, "トークナイズできません");
  }

  // while が完了したら終了を表す TK_EOF を tokens[i].ty に格納
  // tokens[i].input には、終端文字を表す \0 が入る(と思う)
  tokens[i].ty = TK_EOF;
  tokens[i].input = p;
}

void gen(Node *node) {
  if (node->ty == ND_NUM) {
    printf("  push %d\n", node->val);
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  pop rdi\n");
  printf("  pop rax\n");

  switch (node->ty) {
    case '+':
      printf("  add rax, rdi\n");
      break;
    case '-':
      printf("  sub rax, rdi\n");
      break;
    case '*':
      printf("  imul rdi\n");
      break;
    case '/':
      printf("  cqo\n");
      printf("  idiv rdi\n");
      break;
    case '%':
      printf("  push 0\n");
      printf("  pop rdx\n");
      printf("  idiv rdi\n");
      printf("  push rdx\n");
      printf("  pop rax\n");
      break;
    case TK_EQ:
      printf("  cmp rax, rdi\n");
      printf("  sete al\n");
      printf("  movzb rax, al\n");
      break;
    case TK_NE:
      printf("  cmp rax, rdi\n");
      printf("  setne al\n");
      printf("  movzb rax, al\n");
      break;
    case '<':
    case '>':
      printf("  cmp rax, rdi\n");
      printf("  setl al\n");
      printf("  movzb rax, al\n");
      break;
    case TK_LE:
    case TK_GE:
      printf("  cmp rax, rdi\n");
      printf("  setle al\n");
      printf("  movzb rax, al\n");
      break;
  }

  printf("  push rax\n");
}

int main(int argc, char **argv) {
  // 今現在引数はクォートで囲んだ計1つの引数しか取れないため、
  // コマンドと合わせて2つでないとエラーを返す
  if (argc != 2) {
    error("引数の個数が正しくありません");
    return 1;
  }

  if (strcmp("-test", argv[1]) == 0) {
    runtest();
    return 0;
  }

  // トークナイズする
  // 配列 argv は何かと言うと、 main 関数で取得している引数となる
  // 何の引数かという話だが、このコマンド自体の引数に他ならない
  // つまり './9cc' と、その後に続く式のことだ
  //
  // argv 配列は 'char **argv' で定義されている
  // 配列とポインタは書き方が違うだけで書き換えが可能 だと解釈してよい(と思う)
  // **argv というのは *argv[] とも書くことができ、つまりポインタの配列を定義していることとなる
  //
  // まずはポインタの基本的な考え方だが、 '型 間接演算子(*) 変数名' でポインタが定義できる
  // 通常の変数と違うところは、変数名を指定した際に返ってくる値だ
  // 通常の変数は、その変数を定義するとその型に合わせたメモリを確保し、そこに値を格納できるようになる
  // プログラム上ではその変数名を使い確保したメモリにアクセスする
  // ポインタも似たようなものだが、確保したメモリには別のメモリのアドレスが格納される
  // その別のメモリのアドレスに値を格納できる
  // しかし通常の変数のようにただ変数名を指定してもメモリのアドレスしか引けない
  // そこで利用するのが間接演算子となる
  // 間接演算子を使うと、変数の値にあるメモリアドレスを参照し、そのメモリアドレスに格納されている値を引くことができる
  // ポインタに値を格納する時も、引いてくる時も間接演算子が必要になる
  //
  // で、今回はその関節演算子が二連続で存在している
  // どういうことかというと、なんてことはなく、ポインタのポインタを定義しているだけだ
  // ポインタのネストであるが、これを多重間接参照(ポインタのポインタとも)と呼ぶ
  // で、Cでは p[i] と、 *(p+i) は同等の意味を持つ
  // この+iは見て分かる通り配列の番号を意味しており、これは意味合い的には 
  // メモリアドレスの値 + 宣言した際の型のバイト数 * i というものだ
  // コードにすると *argv[i] は *(*(argv+i)) という形に変換可能となる
  // ポインタで参照したアドレスを更にポインタで参照する形である
  //
  // 実際に値にアクセスする時は、以下のような流れになる(メモリアドレスは適当)
  // ./9cc '1 + 1' だとすると
  // *argv[0](0x0000a000) -> 0x0000a000(0x0000b000) -> 0x0000b000(.) -> .
  // *argv[1](0x0000a008) -> 0x0000a008(0x0000b00c) -> 0x0000b00c(1) -> 1
  // argv[0](0x0000a000) -> 0x0000a000(0x0000b000) -> 0x0000b000
  // argv[1](0x0000a008) -> 0x0000a008(0x0000b00c) -> 0x0000b00c
  // *argv(0x0000a000) -> 0x0000a000(0x0000b000) -> 0x0000b000
  //
  // 上記以外に argv 自体も char 型ポインタで、 &argv[0] を指しているとかもある
  // 書いていくとわけわからなくなっちゃうね
  user_input = argv[1];
  tokenize();

  // パースする
  Node *node = expr();

  // アセンブリの前半部分を出力
  printf(".intel_syntax noprefix\n");
  printf(".global main\n");
  printf("main:\n");

  // 抽象構文木を下りながらコード生成
  gen(node);

  // スタックトップに式全体の値が残っているはずなので
  // それをRAXにロードして関数からの返り値とする
  printf("  pop rax\n");
  printf("  ret\n");
  return 0;
}

