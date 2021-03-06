#ifndef __TOOLS_H
#define __TOOLS_H

#include <string>
#include <cstring>
using namespace std;

//字典树节点
struct trie_node
{
    int value;
    trie_node *ch[130];

    trie_node()
    {
        value = -1;
        memset(ch, 0, sizeof(ch));
    }
};

//字典树
class Trie
{
public:
    Trie();
    ~Trie();

private:
    //根节点
    trie_node *root;
    //递归删除字典树
    void del(trie_node *node);

public:
    //插入
    void insert(string s, int val);
    //搜索
    int search(string s);
};

Trie::Trie()
{
    root = new trie_node();
}

Trie::~Trie()
{
    del(root);
}

void Trie::del(trie_node *node)
{
    if (node == NULL) return;
    for (int i = 0; i < 130; ++i)
    {
        del(node->ch[i]);
    }
    delete node;
}

void Trie::insert(string s, int val)
{
    trie_node *now = root;
    for (int i = 0; i < s.length(); ++i)
    {
        int t = s[i];
        if (now->ch[t] == NULL)
        {
            now->ch[t] = new trie_node();
        }
        now = now->ch[t];
    }
    now->value = val;
}

int Trie::search(string s)
{
    trie_node *now = root;
    for (int i = 0; i < s.length(); ++i)
    {
        int t = s[i];
        if (now->ch[t] != NULL)
        {
            now = now->ch[t];
        }
        else
        {
            return -1;
        }
    }
    return now->value;
}

const char split_char = 2;
const char end_char = 3;

//命令解析时的辅助读函数
int read_1(char *s1, int pos, char *buf)
{
    int now = 0;
    while (s1[pos] != split_char && s1[pos] != end_char)
    {
        buf[now] = s1[pos];
        ++now;
        ++pos;
    }
    buf[now] = '\0';
    return pos + 1;
}

#endif