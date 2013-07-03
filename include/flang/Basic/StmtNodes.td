class AttrSubject;

class Stmt<bit abstract = 0> : AttrSubject {
  bit Abstract = abstract;
}

class DStmt<Stmt base, bit abstract = 0> : Stmt<abstract> {
  Stmt Base = base;
}

// Statements
def ConstructPartStmt : Stmt;
def DeclStmt : Stmt;
def BundledCompoundStmt : Stmt;
def ProgramStmt : Stmt;
def EndProgramStmt : Stmt;
def UseStmt : Stmt;
def ImportStmt : Stmt;
def ImplicitStmt : Stmt;
def DimensionStmt : Stmt;
def FormatStmt : Stmt;
def EntryStmt : Stmt;
def AsynchronousStmt : Stmt;
def ExternalStmt : Stmt;
def IntrinsicStmt : Stmt;
def DataStmt : Stmt;

// executable statements
def BlockStmt  : Stmt;
def AssignStmt : Stmt;
def AssignedGotoStmt : Stmt;
def GotoStmt : Stmt;
def IfStmt : Stmt;
def CFBlockStmt : Stmt<1>;
def DoStmt : DStmt<CFBlockStmt>;
def DoWhileStmt : DStmt<CFBlockStmt>;
def ContinueStmt : Stmt;
def StopStmt : Stmt;
def ReturnStmt : Stmt;
def CallStmt : Stmt;
def AssignmentStmt : Stmt;
def PrintStmt : Stmt;
def WriteStmt : Stmt;
