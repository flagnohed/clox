## Statements

### Plan
statement      → exprStmt
               | forStmt
               | ifStmt
               | printStmt
               | returnStmt
               | whileStmt
               | block ;

declaration    → classDecl
               | funDecl
               | varDecl
               | statement ;

### Current
statement      → exprStmt
               | printStmt ;

declaration    → varDecl
               | statement ;