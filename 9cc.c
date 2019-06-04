#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// トークンの型を表す値
enum {
  TK_NUM = 256, // 整数トークン
  TK_EOF,       // 入力の終わりを表すトークン
  ND_NUM = 256, // 整数のノードの型
};

typedef struct Node {
  int ty;           // 演算子かND_NUM
  struct Node *lhs; // 左辺
  struct Node *rhs; // 右辺
  int val;          // tyがND_NUMの場合のみ使う
} Node;

// トークンの型 Token で宣言可能になる
typedef struct {
  int ty;       // type;  トークンの型
  int val;      // value; tyがTK_NUMの場合、その数値
  char *input;  // input; トークン文字列 (エラーメッセージ用)
} Token;

// 入力プログラム
// ポインタ変数として宣言
// これでchar型(1byte)のポインタ値(メモリアドレス)を格納できるようになった
char *user_input;

// トークナイズした結果のトークン列はこの配列に保存する
// 100個以上のトークンは来ないものとする
Token tokens[100];

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

Node *new_node_num(int val) {
  Node *node = malloc(sizeof(Node));
  node->ty = ND_NUM;
  node->val = val;
  return node;
}

Node *new_node(int ty, Node *lhs, Node *rhs) {
  Node *node = malloc(sizeof(Node));
  node->ty = ty;
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

int pos = 0;

int consume(int ty) {
  if (tokens[pos].ty != ty) {
    return 0;
  }

  pos++;
  return 1;
}

Node *num() {
  // +/-/*/[/]でなければ数値のはず
  if (tokens[pos].ty == TK_NUM) {
    return new_node_num(tokens[pos++].val);
  }

  error_at(tokens[pos].input, "数値ではないトークンです");
}

Node *mul() {
  Node *node = num();

  for(;;) {
    if (consume('*')) {
      node = new_node('*', node, num());
    } else if (consume('/')) {
      node = new_node('/', node, num());
    } else {
      return node;
    }
  }
}

Node *expr() {
  Node *node = mul();

  for(;;) {
    if (consume('+')) {
      node = new_node('+', node, mul());
    } else if (consume('-')) {
      node = new_node('-', node, mul());
    } else {
      return node;
    }
  }
}

void tokenize() {
  // char型のポインタpを宣言して、user_inputの値(メモリアドレス)を格納する
  // これで p を間接演算子で表示(*p)すると、user_inputのメモリアドレスに飛び、
  // そこから更に先の値(argv[1])を参照することができる
  char *p = user_input;

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

    if (*p == '+' || *p == '-' || *p == '*' || *p == '/') {
      tokens[i].ty = *p;
      tokens[i].input = p;
      i++;
      p++;
      continue;
    }

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

  // トークナイズする
  // 引数は1つなのでargv[1](メモリアドレス)をuser_input(ポインタで宣言されている)に代入
  // argv[n] にはスペースで区切られた各コマンドライン文字列・・・は格納されていない
  // 格納されているのは、文字列が格納されているメモリアドレスである
  // 例えばargv[1]に 0x0000ff0c というアドレスが格納されているとする
  // そのメモリアドレスには第一引数の '1 + 1' が格納されている
  // 単純に argv[1] の値を引っ張ってくると 0x0000ff0c が出てくるわけだが、
  // ポインタを使えば格納されている値をアドレスと解釈し、そのアドレスを参照し、
  // アドレスの先までアクセスして値を引っ張ってきてくれる
  // つまり *argv[1](0x0000ff0c) -> 0x0000ff0c(1 + 1) という風に格納されている値を参照し、
  // 更に先に格納されている値を引っ張ってきてくれる
  // これがつまり *argv[1] としたときに '1 + 1' が表示される理由である
  //
  // ついでに言うと argv 変数それ自体もポインタである
  // つまり argv それ自体はただのchar型ポインタであり、
  // argv には 0x0000a000 のようなアドレスが格納されているのみである
  // これが何なのかと言うと、argv という変数のメモリアドレスである
  // 通常配列では先頭の文字列のアドレスが0に格納されるが、これは違う
  // 決して argv[0] に格納されている './9cc' の '.' を示しているものではない
  // こんがらがるので再度説明する
  // 上でargv[1]は0x0000ff0cが格納されていると説明した
  // つまりargv[1] == 0x0000ff0cではあるが、&argv[1] != 0c0000ff0cではない
  // argv[1]変数自体を格納しているアドレスは別にあるということだ
  // そこでargvなのだが、これが argv[0] それ自体のアドレスを指し示すポインタとなっている
  // 図解すると以下のようになる
  // argv(0x0000a000) -> 0x0000a000(argv[0]) -> argv[0](0x0000ff00)
  // だもんで argv == 0x0000a000, *argv == 0x0000ff00 となる
  // で、Cでは p[i] と、 *(p+i) は同等の意味を持つ
  // この+iは見て分かる通り配列を意味しており、これは意味合い的には 
  // メモリアドレスの値 + 宣言した際の型のバイト数 * i というものだ
  // char型の場合は1バイトなので、+1すると1バイト、つまり8ビットアドレスを進めることになる
  // 今回の場合は、argv[1] == 0x0000a002 となる
  //
  // なので argv[0] とした時には *(argv+0) と同じ意味(つまり *argv)になるので
  // 0x0000a000の値を取ってくるし、argv[1]とした場合は *(argv+1) となるため、
  // argv[1](0x0000a002) -> 0x0000a002(0x0000ff0c) -> 0x0000ff0c('1 + 1'の先頭である"1"が格納されているアドレス)となる
  // ただ、ここで引っ張ってきているのは単なるアドレスであり、肝心の"1"ではない
  // そのため、このアドレスから先を引っ張ってくるために更にポインタを使う
  // ポインタのネストであるが、これを多重間接参照(ポインタのポインタとも)と呼ぶ
  // コードにすると *argv[i] は *(*(argv+i) という形となる
  // ポインタで参照したアドレスを更にポインタで参照する形である
  // ややこしいね！
  // 長くなったが、下でやっているのは user_input 変数に argv[1] の値を代入する行為だ
  // argv[1] の値はメモリアドレスである
  // これにより *user_input を指定した時に、 argv[1] の値(1 + 1)にアクセスしに行くことができる
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

