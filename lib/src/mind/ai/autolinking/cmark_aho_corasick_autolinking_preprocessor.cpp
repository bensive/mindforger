/*
 cmark_aho_corasick_autolinking_preprocessor.cpp     MindForger thinking notebook

 Copyright (C) 2016-2019 Martin Dvorak <martin.dvorak@mindforger.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "cmark_aho_corasick_autolinking_preprocessor.h"

namespace m8r {

using namespace std;

CmarkAhoCorasickAutolinkingPreprocessor::CmarkAhoCorasickAutolinkingPreprocessor(Mind& mind)
    : AutolinkingPreprocessor{mind}
{
}

CmarkAhoCorasickAutolinkingPreprocessor::~CmarkAhoCorasickAutolinkingPreprocessor()
{
}

void CmarkAhoCorasickAutolinkingPreprocessor::process(const std::vector<std::string*>& md, std::vector<std::string*>& amd)
{
#ifdef MF_MD_2_HTML_CMARK

#ifdef DO_MF_DEBUG
    MF_DEBUG("[Autolinking] begin CMARK-AHO" << endl);
    auto begin = chrono::high_resolution_clock::now();
#endif

    updateIndices();
    insensitive = Configuration::getInstance().isAutolinkingCaseInsensitive();

    if(md.size()) {

        // IMPROVE measure time in here and if over give limit, than STOP injecting
        // and leave i.e. what happens is that a time SLA will be fulfilled and
        // some part (prefix) of the input MD will be autolinked.

        bool inCodeBlock=false, inMathBlock=false;
        for(string* l:md) {
            // every line is autolinked SEPARATELY

            string* nl = new string{};

            // skip code/math/... blocks
            if(stringStartsWith(*l, CODE_BLOCK)) {
                inCodeBlock = !inCodeBlock;

                nl->append(*l);
                amd.push_back(nl);
                continue;
            } else if(stringStartsWith(*l, MATH_BLOCK)) {
                inMathBlock= !inMathBlock;

                nl->append(*l);
                amd.push_back(nl);
                continue;
            }

            if(l && l->size()) {
                MF_DEBUG(">>" << *l << ">>" << endl);
                parseMarkdownLine(l, nl);
                MF_DEBUG("<<" << *nl << "<<" << endl);
                amd.push_back(nl);
            } else {
                amd.push_back(nl);
            }
        }
    }
#ifdef DO_MF_DEBUG
    auto end = chrono::high_resolution_clock::now();
    MF_DEBUG("[Autolinking] done - MD autolinked in: " << chrono::duration_cast<chrono::microseconds>(end-begin).count()/1000000.0 << "ms" << endl);
#endif

#else
    amd = md;
#endif
}

void CmarkAhoCorasickAutolinkingPreprocessor::parseMarkdownLine(std::string* md, std::string* amd)
{
#ifdef DO_MF_DEBUG
    MF_DEBUG("[Autolinking] parsing line '" << *md << "'" << endl);
#endif

#ifdef MF_MD_2_HTML_CMARK
    const char* smd = md->c_str();
    cmark_node* document = cmark_parse_document(
        smd,
        strlen(smd),
        CMARK_OPT_DEFAULT);

    MF_DEBUG(endl);

    // AST iteration

    cmark_event_type eventType;
    cmark_iter *i = cmark_iter_new(document);

    string itxt{}, otxt{};

    cmark_node* zombieNode{};

    while ((eventType = cmark_iter_next(i)) != CMARK_EVENT_DONE) {
        cmark_node *node = cmark_iter_get_node(i);

        if(zombieNode) {
            cmark_node_unlink(zombieNode);
            cmark_node_free(zombieNode);
            zombieNode = nullptr;
        }

        // do something with `cur` and `ev_type`
        switch(eventType) {
        case CMARK_EVENT_ENTER:
            cout << "ENTER";
            break;
        case CMARK_EVENT_EXIT:
            cout << "LEAVE";
            break;
        case CMARK_EVENT_DONE:
            cout << "DONE";
            break;
        case CMARK_EVENT_NONE:
            cout << "NONE";
            break;
        default:
            cout << ".";
        }

        switch(cmark_node_get_type(node)) {
        case CMARK_NODE_CODE:
            cout << " code";
            break;
        case CMARK_NODE_LINK:
            cout << " link";
            break;
        case CMARK_NODE_IMAGE:
            cout << " image";
            break;
        case CMARK_NODE_TEXT:
            cout << " text " << cmark_node_get_literal(node);

            // replace text node w/ sequence of text and link nodes
            injectThingsLinks(node);
            zombieNode = node;

            break;
        default:
            cout << " .";
        }

        cout << endl;
    }

    cmark_iter_free(i);

    // Nodes must only be modified after an `EXIT` event,
    // or an `ENTER` event for leaf nodes.

    // inlined autolinking constructions to avoid - auto-link in:
    // - link
    // - image
    // - code
    // ... iterate nodes and skip <text/> if UNDER link/image/code,
    // otherwise trim text and try to autolink it.
    //
    // Set autolinked text and then try to serialize to MD, it should
    // stay there.

#ifdef DO_MF_DEBUG
    char* xml = cmark_render_xml(document, 0);
    MF_DEBUG("[Autolinking] Line's cmark AST as XML:" << endl << endl);
    MF_DEBUG(xml << endl);
    free(xml);

    char* txt = cmark_render_commonmark(document, 0, 100);
    MF_DEBUG("[Autolinking] Line's cmark AST as MD:" << endl << endl);
    MF_DEBUG(txt << endl);
    free(txt);
#endif

    // TODO set correct
    char* cmm = cmark_render_commonmark(document, 0, 0);
    amd->assign(cmm);
    free(cmm);
#else
    // TODO copy md to amd
#endif
}

void CmarkAhoCorasickAutolinkingPreprocessor::injectThingsLinks(cmark_node* origNode)
{
    // copy w to t as it will be chopped word/match by word/match from head to tail
    string w{cmark_node_get_literal(origNode)}, at{}, chop{};

    cmark_node* node{};
    cmark_node* linkNode{};
    cmark_node* txtNode{};

#ifdef DO_MF_DEBUG
    MF_DEBUG("[Autolinking] Injecting links to: '" << w << "'" << endl);
#endif

    while(w.size()>0) {
        // find match which is PREFIX of chopped line
        bool linked = false;

        // IMPROVE rewrite string search to Aho-Corasic trie

        // IMPROVE existing (non-Aho-Corasic) algorithm can be optimized:
        // - sort Os/Ns alphabetically
        //     - search structure: map w/ leading char as key and list of
        //       Os/Ns names ordered by length as value
        //         - have this for both case in/sensitive cases ~ 2 structs
        //     - sort words with the same 1st word by length
        //     - remember begin-end of words starting with the char
        // - determine first w character
        //     - search only Os/Ns w/ that particular 1st character only
        // ... this will speed up search significantly, perhaps it can be
        // 10x faster than search for all Os/Ns names

        // inject Os, then Ns
        for(Thing* t:things) {
            size_t found;
            bool match, insensitiveMatch;
            string lowerAlias{};

            // FIND: accept occurences in the HEAD of word only
            if((found=w.find(t->getAutolinkingAlias()))!=string::npos
                  &&
                !found)
            {
                match = true; insensitiveMatch = false;
            } else {
                lowerAlias.assign(t->getAutolinkingAlias());
                lowerAlias[0] = std::tolower(t->getAutolinkingAlias()[0]);

                // FIND: accept occurences in the HEAD of word only
                if(insensitive
                     &&
                   (found=w.find(lowerAlias))!=string::npos
                     &&
                   !found)
                {
                    match = insensitiveMatch = true;
                } else {
                    match = false;
                }
            }

            if(match) {
                // avoid word PREFIX matches ~ ensure that WHOLE world is matched

                string m{" \t,:;.!?<>{}&()-+/*"};
                // determine trailing char c
                char c{w.size()==t->getAutolinkingAlias().size()?' ':w.at(t->getAutolinkingAlias().size())};
                MF_DEBUG("  c: '" << c << "'" << endl);
                if(w.size()==t->getAutolinkingAlias().size()
                     ||
                   m.find(c)!=string::npos)
                {
                    linked = true;

                    // IMPROVE make this method
                    // AST: add text node w/ content preceding link
                    if(at.size()) {
                        txtNode = cmark_node_new(CMARK_NODE_TEXT);
                        cmark_node_set_literal(txtNode, at.c_str());
                        // IMPROVE make this private method
                        if(node) {
                            cmark_node_insert_after(node, txtNode);
                        } else {
                            cmark_node_insert_before(origNode, txtNode);
                        }
                        node = txtNode;

                        at.clear();
                    }

                    // IMPROVE make this method
                    // AST: add link
                    linkNode = cmark_node_new(CMARK_NODE_LINK);
                    cmark_node_set_url(linkNode, t->getKey().c_str());
                    txtNode = cmark_node_new(CMARK_NODE_TEXT);
                    cmark_node_set_literal(txtNode, insensitiveMatch?lowerAlias.c_str():t->getAutolinkingAlias().c_str());
                    cmark_node_append_child(linkNode, txtNode);
                    if(node) {
                        cmark_node_insert_after(node, linkNode);
                    } else {
                        cmark_node_insert_before(origNode, linkNode);
                    }
                    node = linkNode;

                    // append trailing char
                    at += c;

                    // chop linked prefix word from input word w
                    w = w.substr(t->getAutolinkingAlias().size()+(w.size()==t->getAutolinkingAlias().size()?0:1));

                    // leave Os/Ns loop on the first match
                    break;
                }
            }
        }

        //
        MF_DEBUG("   l>" << std::boolalpha << linked << endl);
        if(linked) {
            // prefix has been linked + chopped
            // IMPROVE trailing should be already appended above
            // IMPROVE SPACE vs. TAB
            at.append(" ");
        } else {
            // current w prefix was NOT linked > chop it and append it
            size_t begin = w.find_first_of(" \t");
            if(begin != string::npos) {
                chop = w.substr(0, begin);
                w = w.substr(begin+1);
                at.append(chop);
                at.append(" ");

                MF_DEBUG("  -c>" << chop << endl);
                MF_DEBUG("   w>" << w << endl);
                MF_DEBUG("   <<" << at << endl);
            } else {
                // no more words (prefix already checked) > DONE
                at.append(w);

                MF_DEBUG("   w>" << w << endl);
                MF_DEBUG("   <<" << at << endl);

                break;
            }
        }
    }

    // AST: add text node w/ content preceding link
    if(at.size()) {
        txtNode = cmark_node_new(CMARK_NODE_TEXT);
        cmark_node_set_literal(txtNode, at.c_str());
        if(node) {
            cmark_node_insert_after(node, txtNode);
        } else {
            cmark_node_insert_before(origNode, txtNode);
        }
        node = txtNode;

        // not required: at.clear();
    }
}

} // m8r namespace
