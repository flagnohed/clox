## Statements

### Plan
statement      → exprStmt
               | forStmt
               | ifStmt
               | printStmt
               | returnStmt
               | whileStmt
               | block ;

block          → "{" declaration* "}" ;

declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;

### Current
statement      → exprStmt
               | printStmt ;
               | block ;

block          → "{" declaration* "}" ;

declaration    → varDecl
               | statement ;

Den tror att fib är n ??

