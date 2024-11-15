
#include <catch2/catch_test_macros.hpp>
#include "CompileTool/CompileTransitionGraph/PartitionGraphParser.hpp"
#include <iostream>
#include <random>
#include <string>


using namespace CorGIReg;

    std::string generateRandomString() {
        //std::random_device rd;
        std::mt19937 gen(1021655632);
        std::uniform_int_distribution<int> dis('A', 'Z');

        std::string randomString;
        randomString += static_cast<char>(dis(gen));
        randomString += static_cast<char>(dis(gen));

        return randomString;
    }

    //generative function ,
    std::string domain();
    std::string exp() {
        int randomValue = std::rand() % 7;
        switch (randomValue) {
            case 0:
                return generateRandomString();
            case 1:
                return generateRandomString()+"*";
            case 2:
                return generateRandomString()+"+";
            case 3:
                return generateRandomString()+"?";
            case 4 :
                return generateRandomString()+"#";
            case 5 :
                return generateRandomString()+"$";
            default:
                return domain();
        }
    }

   std::string rseq() {
        int randomValue = std::rand() % 2;
        switch (randomValue) {
            case 0:
                return exp();
            default:
                return  exp()+"<-"+rseq();
        }
    }

    std::string seq() {
        int randomValue = std::rand() % 3;
        switch (randomValue) {
            case 0:
                return exp();
            case 1:
                return  exp()+"->"+seq();
            case 2:
                return  exp()+"<-"+rseq();  
            case 3:
                return  "$<-"+rseq();
            default:
                return exp()+"->$";
        }
    }

    std::string domain() {
        int randomValue = std::rand() % 3;

        switch (randomValue) {
            case 0:
                return  "("+ seq() +")*";
            case 1:
                return  "("+ seq() +")?";
            default:
                return  "("+ seq() +")+";
        }
    }

    std::string allExpr() {
        int randomValue = std::rand() % 2;
        switch (randomValue) {
            case 0:
                return  seq();
            default :
                return  seq()+ ";" +allExpr();
        }
    }

/*
exp : KEY(QOM | QZM)?  | CKEY | domain
seq :exp (NEXT seq)* 
domain :  LPAREN seq RPAREN (QOM | QZM) 
allExpr: seq (SEP allExpr)*
*/



TEST_CASE("PartitionGraphParser", "Test_GraphParser") {

    SECTION("Correct parsing") {
        for (int i = 0; i < 100; ++i) {
            const std::string test = allExpr();
            //std::cout << test <<"\n";
            PartitionGraphParser graphParser = PartitionGraphParser(test);
            std::shared_ptr<AstNode<tGTokensTypes>> tree = graphParser.parse();
        }
    }

    SECTION("Throws END token") {
        REQUIRE_THROWS(PartitionGraphParser("A->$->B").parse());
        REQUIRE_THROWS(PartitionGraphParser("$->B").parse());
        REQUIRE_THROWS(PartitionGraphParser("$#").parse());
        REQUIRE_THROWS(PartitionGraphParser("$").parse());
        REQUIRE_THROWS(PartitionGraphParser("A#$").parse());
    }

    SECTION("Throws PREC token") {
        REQUIRE_THROWS(PartitionGraphParser("<-A").parse());
        REQUIRE_THROWS(PartitionGraphParser("A<-B->C").parse());
        REQUIRE_THROWS(PartitionGraphParser("A<-$").parse());
    }
}