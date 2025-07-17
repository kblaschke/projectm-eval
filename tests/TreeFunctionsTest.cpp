#include "TreeFunctionsTest.hpp"

#include <cmath>

#ifdef _MSC_VER
#define strcasecmp stricmp
#define strncasecmp _strnicmp
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
    #define M_PI_2 1.5707963267948966
#endif

#if PRJM_F_SIZE == 4
#define ASSERT_PRJM_F_EQ ASSERT_FLOAT_EQ
#define EXPECT_PRJM_F_EQ EXPECT_FLOAT_EQ
#else
#define ASSERT_PRJM_F_EQ ASSERT_DOUBLE_EQ
#define EXPECT_PRJM_F_EQ EXPECT_DOUBLE_EQ
#endif

prjm_eval_variable_def_t* TreeFunctions::FindVariable(const char* name)
{
    for (const auto var: m_variables)
    {
        if (strcasecmp(name, var->name) == 0)
        {
            return var;
        }
    }

    return nullptr;
}

prjm_eval_variable_def_t* TreeFunctions::CreateVariable(const char* name, PRJM_EVAL_F initialValue)
{
    auto* var = FindVariable(name);
    if (!var)
    {
        var = new prjm_eval_variable_def_t;
    }

    var->name = strdup(name);
    var->value = initialValue;

    m_variables.push_back(var);

    return var;
}

prjm_eval_exptreenode_t* TreeFunctions::CreateEmptyNode(int argCount)
{
    auto* node = reinterpret_cast<prjm_eval_exptreenode_t*>(calloc(1, sizeof(prjm_eval_exptreenode_t)));
    if (argCount > 0)
    {
        node->args = reinterpret_cast<prjm_eval_exptreenode_t**>( calloc(argCount + 1,
                                                                         sizeof(prjm_eval_exptreenode_t*)));
    }
    return node;
}

prjm_eval_exptreenode_t* TreeFunctions::CreateConstantNode(PRJM_EVAL_F value)
{
    auto* varNode = CreateEmptyNode(0);

    varNode->func = prjm_eval_func_const;
    varNode->value = value;

    return varNode;
}

prjm_eval_exptreenode_t*
TreeFunctions::CreateVariableNode(const char* name, PRJM_EVAL_F initialValue, prjm_eval_variable_def_t** variable)
{
    *variable = CreateVariable(name, initialValue);

    auto* varNode = CreateEmptyNode(0);

    varNode->func = prjm_eval_func_var;
    varNode->var = &(*variable)->value;

    return varNode;
}

void TreeFunctions::SetUp()
{
    Test::SetUp();
}

void TreeFunctions::TearDown()
{
    for (auto node: m_treeNodes)
    {
        prjm_eval_destroy_exptreenode(node);
    }
    m_treeNodes.clear();

    for (const auto var: m_variables)
    {
        free(var->name); // alloc'd via C malloc/strdup!
        delete var;
    }
    m_variables.clear();

    if (m_memoryBuffer != nullptr)
    {
        prjm_eval_memory_destroy_buffer(m_memoryBuffer);
        m_memoryBuffer = nullptr;
    }

    Test::TearDown();

}


TEST_F(TreeFunctions, Constant)
{
    auto* constNode = CreateConstantNode(5.0f);

    m_treeNodes.push_back(constNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    constNode->func(constNode, &valuePointer);

    ASSERT_PRJM_F_EQ(*valuePointer, 5.);
}

TEST_F(TreeFunctions, Variable)
{
    prjm_eval_variable_def_t* var;
    auto* varNode = CreateVariableNode("x", 5.f, &var);

    m_treeNodes.push_back(varNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    varNode->func(varNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 5.);
    ASSERT_EQ(valuePointer, &var->value);
}

TEST_F(TreeFunctions, ExecuteList)
{

    // Expression list ("x = -50; y = 50;")
    prjm_eval_variable_def_t* var1;
    auto* varNode1 = CreateVariableNode("x", 5.f, &var1);
    auto* constNode1 = CreateConstantNode(-50.0f);

    auto* setNode1 = CreateEmptyNode(2);
    setNode1->func = prjm_eval_func_set;
    setNode1->args[0] = varNode1;
    setNode1->args[1] = constNode1;


    prjm_eval_variable_def_t* var2;
    auto* varNode2 = CreateVariableNode("y", 123.f, &var2);
    auto* constNode2 = CreateConstantNode(50.0f);

    auto* setNode2 = CreateEmptyNode(2);
    setNode2->func = prjm_eval_func_set;
    setNode2->args[0] = varNode2;
    setNode2->args[1] = constNode2;

    auto* listItem = new prjm_eval_exptreenode_list_item_t{};
    listItem->expr = setNode1;
    listItem->next = new prjm_eval_exptreenode_list_item_t{};
    listItem->next->expr = setNode2;

    // Executor
    auto* listNode = CreateEmptyNode(1);
    listNode->func = prjm_eval_func_execute_list;
    listNode->list = listItem;

    m_treeNodes.push_back(listNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    listNode->func(listNode, &valuePointer);

    // Last executed node (y = 50) is return value
    EXPECT_PRJM_F_EQ(*valuePointer, 50.);

    // Both constants should now be assigned to each respective variable
    EXPECT_PRJM_F_EQ(var1->value, -50.);
    EXPECT_PRJM_F_EQ(var2->value, 50.);
}

TEST_F(TreeFunctions, ExecuteLoop)
{
    // Test expression: "loop(42, x += 1)" with x starting at 0.
    prjm_eval_variable_def_t* varX;
    auto* varNodeX = CreateVariableNode("x", 0.f, &varX);
    auto* constNode1 = CreateConstantNode(1.0f);
    auto* constNode42 = CreateConstantNode(42.0f);

    auto* incrementNode1 = CreateEmptyNode(2);
    incrementNode1->func = prjm_eval_func_add_op;
    incrementNode1->args[0] = varNodeX;
    incrementNode1->args[1] = constNode1;

    // Executor
    auto* loopNode = CreateEmptyNode(2);
    loopNode->func = prjm_eval_func_execute_loop;
    loopNode->args[0] = constNode42;
    loopNode->args[1] = incrementNode1;

    m_treeNodes.push_back(loopNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    loopNode->func(loopNode, &valuePointer);

    // Last executed node ("41 += 1" resulting in 42) is return value
    EXPECT_PRJM_F_EQ(*valuePointer, 42.);
    EXPECT_PRJM_F_EQ(varX->value, 42.);
}

TEST_F(TreeFunctions, ExecuteWhile)
{
    // Test expression: "while(x -= 1)" with x starting at 42.
    prjm_eval_variable_def_t* varX;
    auto* varNodeX = CreateVariableNode("x", 42.f, &varX);
    auto* constNode1 = CreateConstantNode(1.0f);

    auto* decrementNode1 = CreateEmptyNode(2);
    decrementNode1->func = prjm_eval_func_sub_op;
    decrementNode1->args[0] = varNodeX;
    decrementNode1->args[1] = constNode1;

    // Executor
    auto* whileNode = CreateEmptyNode(1);
    whileNode->func = prjm_eval_func_execute_while;
    whileNode->args[0] = decrementNode1;

    m_treeNodes.push_back(whileNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    whileNode->func(whileNode, &valuePointer);

    // Last executed node ("1 -= 1" resulting in 0) is return value
    EXPECT_PRJM_F_EQ(*valuePointer, 0.);
    EXPECT_PRJM_F_EQ(varX->value, 0.);
}

TEST_F(TreeFunctions, If)
{
    // Test expression: "if(x < 10, 42, 666)"
    prjm_eval_variable_def_t* varX;
    auto* varNodeX = CreateVariableNode("x", 0.f, &varX);
    auto* constNode10 = CreateConstantNode(10.0f);
    auto* constNode42 = CreateConstantNode(42.0f);
    auto* constNode666 = CreateConstantNode(999.0f);

    auto* comparisonNode = CreateEmptyNode(2);
    comparisonNode->func = prjm_eval_func_below;
    comparisonNode->args[0] = varNodeX;
    comparisonNode->args[1] = constNode10;

    auto* ifNode = CreateEmptyNode(3);
    ifNode->func = prjm_eval_func_if;
    ifNode->args[0] = comparisonNode;
    ifNode->args[1] = constNode42;
    ifNode->args[2] = constNode666;

    m_treeNodes.push_back(ifNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    ifNode->func(ifNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, constNode42->value);

    varX->value = 1000.;

    ifNode->func(ifNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, constNode666->value);
}

TEST_F(TreeFunctions, IfWithReferenceReturn)
{
    // Test expression: "if(x < 10, 42, y) = 666"
    // Assigns 666 to variable y if x >= 10
    prjm_eval_variable_def_t* varX;
    auto* varNodeX = CreateVariableNode("x", 0., &varX);
    prjm_eval_variable_def_t* varY;
    auto* varNodeY = CreateVariableNode("y", 999., &varY);
    auto* constNode10 = CreateConstantNode(10.);
    auto* constNode42 = CreateConstantNode(42.);
    auto* constNode666 = CreateConstantNode(666.);

    auto* comparisonNode = CreateEmptyNode(2);
    comparisonNode->func = prjm_eval_func_below;
    comparisonNode->args[0] = varNodeX;
    comparisonNode->args[1] = constNode10;

    auto* ifNode = CreateEmptyNode(3);
    ifNode->func = prjm_eval_func_if;
    ifNode->args[0] = comparisonNode;
    ifNode->args[1] = constNode42;
    ifNode->args[2] = varNodeY;

    auto* setNode = CreateEmptyNode(2);
    setNode->func = prjm_eval_func_set;
    setNode->args[0] = ifNode;
    setNode->args[1] = constNode666;

    m_treeNodes.push_back(setNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    setNode->func(setNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, constNode666->value);
    EXPECT_PRJM_F_EQ(varY->value, 999.);

    varX->value = 1000.;

    setNode->func(setNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, constNode666->value);
    EXPECT_PRJM_F_EQ(varY->value, constNode666->value);
}

TEST_F(TreeFunctions, Execute2)
{
    // Expression: "exec2(x = -50, y = 50)"
    // Syntactically identical to just writing "x = -50; y = 50"
    prjm_eval_variable_def_t* var1;
    auto* varNode1 = CreateVariableNode("x", 5.f, &var1);
    auto* constNode1 = CreateConstantNode(-50.0f);

    auto* setNode1 = CreateEmptyNode(2);
    setNode1->func = prjm_eval_func_set;
    setNode1->args[0] = varNode1;
    setNode1->args[1] = constNode1;

    prjm_eval_variable_def_t* var2;
    auto* varNode2 = CreateVariableNode("y", 123.f, &var2);
    auto* constNode2 = CreateConstantNode(50.0f);

    auto* setNode2 = CreateEmptyNode(2);
    setNode2->func = prjm_eval_func_set;
    setNode2->args[0] = varNode2;
    setNode2->args[1] = constNode2;

    auto* exec2Node = CreateEmptyNode(2);
    exec2Node->func = prjm_eval_func_exec2;
    exec2Node->args[0] = setNode1;
    exec2Node->args[1] = setNode2;

    m_treeNodes.push_back(exec2Node);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    exec2Node->func(exec2Node, &valuePointer);

    // Last executed node (y = 50) is return value
    EXPECT_PRJM_F_EQ(*valuePointer, 50.);

    // Both constants should now be assigned to each respective variable
    EXPECT_PRJM_F_EQ(var1->value, -50.);
    EXPECT_PRJM_F_EQ(var2->value, 50.);
}

TEST_F(TreeFunctions, Execute3)
{
    // Expression: "exec2(x = -50, y = 50)"
    // Syntactically identical to just writing "x = -50; y = 50"
    prjm_eval_variable_def_t* var1;
    auto* varNode1 = CreateVariableNode("x", 5.f, &var1);
    auto* constNode1 = CreateConstantNode(-50.0f);

    auto* setNode1 = CreateEmptyNode(2);
    setNode1->func = prjm_eval_func_set;
    setNode1->args[0] = varNode1;
    setNode1->args[1] = constNode1;

    prjm_eval_variable_def_t* var2;
    auto* varNode2 = CreateVariableNode("y", 123.f, &var2);
    auto* constNode2 = CreateConstantNode(50.0f);

    auto* setNode2 = CreateEmptyNode(2);
    setNode2->func = prjm_eval_func_set;
    setNode2->args[0] = varNode2;
    setNode2->args[1] = constNode2;

    prjm_eval_variable_def_t* var3;
    auto* varNode3 = CreateVariableNode("z", 456.f, &var3);
    auto* constNode3 = CreateConstantNode(200.0f);

    auto* setNode3 = CreateEmptyNode(2);
    setNode3->func = prjm_eval_func_set;
    setNode3->args[0] = varNode3;
    setNode3->args[1] = constNode3;

    auto* exec3Node = CreateEmptyNode(3);
    exec3Node->func = prjm_eval_func_exec3;
    exec3Node->args[0] = setNode1;
    exec3Node->args[1] = setNode2;
    exec3Node->args[2] = setNode3;

    m_treeNodes.push_back(exec3Node);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    exec3Node->func(exec3Node, &valuePointer);

    // Last executed node (z = 200) is return value
    EXPECT_PRJM_F_EQ(*valuePointer, 200.);

    // All three constants should now be assigned to each respective variable
    EXPECT_PRJM_F_EQ(var1->value, -50.);
    EXPECT_PRJM_F_EQ(var2->value, 50.);
    EXPECT_PRJM_F_EQ(var3->value, 200.);
}

TEST_F(TreeFunctions, Assignment)
{
    prjm_eval_variable_def_t* var1;
    prjm_eval_variable_def_t* var2;
    auto* varNode1 = CreateVariableNode("x", 5.f, &var1);
    auto* varNode2 = CreateVariableNode("y", 45.f, &var2);

    auto* setNode = CreateEmptyNode(2);
    setNode->func = prjm_eval_func_set;
    setNode->args[0] = varNode1;
    setNode->args[1] = varNode2;

    m_treeNodes.push_back(setNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    setNode->func(setNode, &valuePointer);

    EXPECT_EQ(valuePointer, &var1->value);
    EXPECT_NE(valuePointer, &var2->value);
    EXPECT_PRJM_F_EQ(*valuePointer, 45.0);
    EXPECT_PRJM_F_EQ(var1->value, 45.0);
}

TEST_F(TreeFunctions, MemoryAccess)
{
    // Expression: "mem[42] = 59"
    // megabuf(), gmem[] and gmegabuf() are equivalent, they only get different memory buffer pointers during compilation.
    m_memoryBuffer = prjm_eval_memory_create_buffer();

    auto* constNode42 = CreateConstantNode(42.);
    auto* constNode50 = CreateConstantNode(50.0f);

    auto* memNode = CreateEmptyNode(1);
    memNode->func = prjm_eval_func_mem;
    memNode->memory_buffer = m_memoryBuffer;
    memNode->args[0] = constNode42;

    auto* setNode = CreateEmptyNode(2);
    setNode->func = prjm_eval_func_set;
    setNode->args[0] = memNode;
    setNode->args[1] = constNode50;

    m_treeNodes.push_back(setNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    setNode->func(setNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 50.0);

    PRJM_EVAL_F* memoryPointer = prjm_eval_memory_allocate(m_memoryBuffer, static_cast<int>(constNode42->value));
    ASSERT_NE(memoryPointer, nullptr);
    EXPECT_EQ(*memoryPointer, 50.0);
}

TEST_F(TreeFunctions, FreeMemoryBuffer)
{
    // Expression: "freembuf(10)"
    // No memory should be freed, as this function doesn't do anything in Milkdrop.
    m_memoryBuffer = prjm_eval_memory_create_buffer();

    auto* constNode10 = CreateConstantNode(10.);

    auto* freembufNode = CreateEmptyNode(1);
    freembufNode->func = prjm_eval_func_freembuf;
    freembufNode->memory_buffer = m_memoryBuffer;
    freembufNode->args[0] = constNode10;

    m_treeNodes.push_back(freembufNode);

    // 10th value in block 10
    PRJM_EVAL_F* memoryPointer = prjm_eval_memory_allocate(m_memoryBuffer, 65536 * 10 + 10);
    *memoryPointer = 123.;

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    freembufNode->func(freembufNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 10.);
    EXPECT_PRJM_F_EQ(*memoryPointer, 123.);
}

TEST_F(TreeFunctions, MemoryCopyWithOverlap)
{
    // Expression: "memcpy(65536, 65636, 200)"
    m_memoryBuffer = prjm_eval_memory_create_buffer();

    auto* constNode65536 = CreateConstantNode(65536.);
    auto* constNode65636 = CreateConstantNode(65636.);
    auto* constNode200 = CreateConstantNode(200.);

    auto* memcpyNode = CreateEmptyNode(3);
    memcpyNode->func = prjm_eval_func_memcpy;
    memcpyNode->memory_buffer = m_memoryBuffer;
    memcpyNode->args[0] = constNode65536;
    memcpyNode->args[1] = constNode65636;
    memcpyNode->args[2] = constNode200;

    m_treeNodes.push_back(memcpyNode);

    // Populate the whole range (65536 to 65835) with increasing numbers
    for (int index = 0; index < 300; ++index)
    {
        PRJM_EVAL_F* memoryPointer = prjm_eval_memory_allocate(m_memoryBuffer, 65536 + index);
        ASSERT_NE(memoryPointer, nullptr);
        *memoryPointer = static_cast<PRJM_EVAL_F>(index + 1);
    }

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    memcpyNode->func(memcpyNode, &valuePointer);

    // Destination index is returned
    EXPECT_PRJM_F_EQ(*valuePointer, constNode65536->value);

    // Check memory, should now contain indices 101-300, followed by 201-300.
    // Using assertions here to not spam the output with 300 errors if something failed.
    for (int index = 0; index < 300; ++index)
    {
        PRJM_EVAL_F* memoryPointer = prjm_eval_memory_allocate(m_memoryBuffer, 65536 + index);
        ASSERT_NE(memoryPointer, nullptr);
        if (index < 200)
        {
            ASSERT_EQ(static_cast<int>(*memoryPointer), index + 101);
        }
        else
        {
            ASSERT_EQ(static_cast<int>(*memoryPointer), index + 1);
        }
    }
}

TEST_F(TreeFunctions, DivisionOperator)
{
    prjm_eval_variable_def_t* var1;
    prjm_eval_variable_def_t* var2;
    auto* varNode1 = CreateVariableNode("x", 5., &var1);
    auto* varNode2 = CreateVariableNode("y", 2., &var2);

    auto* divNode = CreateEmptyNode(2);
    divNode->func = prjm_eval_func_div;
    divNode->args[0] = varNode1;
    divNode->args[1] = varNode2;

    m_treeNodes.push_back(divNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    divNode->func(divNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 2.5) << "Dividing 5.0 by 2.0";

    var1->value = 0.0;
    var2->value = 5.0;
    divNode->func(divNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "Dividing 0.0 by 5.0";

    // Division by 0 should return 0, not NaN
    var1->value = 5.0;
    var2->value = 0.0;
    divNode->func(divNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "Dividing 0.0 by 5.0 - expected 0.0, not NaN";
}

TEST_F(TreeFunctions, ASinFunction)
{
    prjm_eval_variable_def_t* var;
    auto* varNode = CreateVariableNode("x", 0.f, &var);

    auto* asinNode = CreateEmptyNode(1);
    asinNode->func = prjm_eval_func_asin;
    asinNode->args[0] = varNode;

    m_treeNodes.push_back(asinNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    asinNode->func(asinNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "asin(0.0)";

    var->value = 1.0;
    asinNode->func(asinNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer,  M_PI_2) << "asin(1.0)";

    var->value = -1.0;
    asinNode->func(asinNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, -M_PI_2) << "asin(-1.0)";

    var->value = 2.0;
    asinNode->func(asinNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "asin(2.0) - expected 0.0, not NaN";

    var->value = -2.0;
    asinNode->func(asinNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "asin(-2.0) - expected 0.0, not NaN";
}

TEST_F(TreeFunctions, ACosFunction)
{
    prjm_eval_variable_def_t* var;
    auto* varNode = CreateVariableNode("x", 0.f, &var);

    auto* acosNode = CreateEmptyNode(1);
    acosNode->func = prjm_eval_func_acos;
    acosNode->args[0] = varNode;

    m_treeNodes.push_back(acosNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    acosNode->func(acosNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, M_PI_2) << "acos(0.0)";

    var->value = 1.0;
    acosNode->func(acosNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer,  0.0) << "acos(1.0)";

    var->value = -1.0;
    acosNode->func(acosNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, M_PI) << "acos(-1.0)";

    var->value = 2.0;
    acosNode->func(acosNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0f) << "acos(2.0) - expected 0.0, not NaN";

    var->value = -2.0;
    acosNode->func(acosNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 0.0f) << "acos(-2.0) - expected 0.0, not NaN";
}

TEST_F(TreeFunctions, SqrFunction)
{
    prjm_eval_variable_def_t* var;
    auto* varNode = CreateVariableNode("x", 0.f, &var);

    auto* sqrNode = CreateEmptyNode(1);
    sqrNode->func = prjm_eval_func_sqr;
    sqrNode->args[0] = varNode;

    m_treeNodes.push_back(sqrNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;
    sqrNode->func(sqrNode, &valuePointer);

    EXPECT_PRJM_F_EQ(*valuePointer, 0.0) << "sqr(0.0)";

    var->value = 1.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 1.0) << "sqr(1.0)";

    var->value = -1.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 1.0) << "sqr(-1.0)";

    var->value = 2.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 4.0) << "sqr(2.0)";

    var->value = -2.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 4.0) << "sqr(-2.0)";

    var->value = 1000000.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 1000000000000.0) << "sqr(1000000.0)";

    var->value = -1000000.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 1000000000000.0) << "sqr(-1000000.0)";

    var->value = 9999999999999.0;
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 4611685743549480960.0) << "sqr(9999999999999.0)";

    // Overflow: should return Inf
#if PRJM_F_SIZE == 4
    // 10^4 below max value for a float
    var->value = 3.402823466E+34f;
#else
    // 10^8 below max value for a double
    var->value =  1.7976931348623157E+300;
#endif
    sqrNode->func(sqrNode, &valuePointer);
    EXPECT_PRJM_F_EQ(*valuePointer, 4611685743549480960.0) << "sqr(" << var->value << ")";

}

TEST_F(TreeFunctions, RandFunction)
{
    prjm_eval_variable_def_t* var;
    auto* varNode = CreateVariableNode("x", 0.f, &var);

    auto* randNode = CreateEmptyNode(1);
    randNode->func = prjm_eval_func_rand;
    randNode->args[0] = varNode;

    m_treeNodes.push_back(randNode);

    PRJM_EVAL_F value{};
    PRJM_EVAL_F* valuePointer = &value;

    for (int i = 0; i < 100; i++)
    {
        randNode->func(randNode, &valuePointer);
        EXPECT_LT(*valuePointer, 1.0) << "rand(0.0)";
    }

    auto testValue = [&var, &randNode, &valuePointer](PRJM_EVAL_F value)
    {
        var->value = value;
        bool oneValueOverThreshold = false;
        PRJM_EVAL_F threshold = value / 2.0;
        for (int i = 0; i < 100; i++)
        {
            randNode->func(randNode, &valuePointer);
            EXPECT_LT(*valuePointer, value) << "rand(" << value << ")";
            if (*valuePointer > threshold)
            {
                oneValueOverThreshold = true;
            }
        }
        EXPECT_TRUE(oneValueOverThreshold) << "One value must be over " << threshold;
    };

    testValue(100.0);
    testValue(10000.0);
    testValue(1000000.0);
    testValue(100000000.0);
    testValue(INT32_MAX);
    testValue(INT32_MAX * 1000000000000000.0);
}