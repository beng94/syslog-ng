#include <cfg-viz.h>

static int count = 0;
static int junc_count = 0;
static int color_count = 31;

const gchar* color[] =
{ "blue", "brown", "coral", "chartreuse", "cadetblue",
  "crimson", "cyan", "darkgreen", "darkorange", "deeppink",
  "gold", "firebrick", "darkslateblue","green", "greenyellow",
  "hotpink", "indigo", "lightcoral", "limegreen", "maroon",
  "navy", "orange", "orangered", "orchid", "peru", "red",
  "royalblue", "tan", "yellow", "violet", "yellowgreen" };

static void
cfg_viz_get_node_id(const LogExprNode *node, gchar* buf, const size_t size)
{
    if(node->content == ENC_PIPE && node->layout == ENL_JUNCTION)
        g_snprintf(buf, size, "secret_head%d", junc_count);
    else
        g_snprintf(buf, size, "%d%s", node->content, node->name);
}

static void
cfg_viz_get_node_props(const LogExprNode *node, const gboolean head, gchar* buf, const size_t size)
{
    if(node->content == ENC_PIPE && node->layout == ENL_JUNCTION)
    {
       if(head) g_snprintf(buf, size, "lhead=cluster_%d__%d", count, junc_count);
       else g_snprintf(buf, size, "ltail=cluster_%d__%d", count, junc_count);
    }
    else buf[0] = '\0';
}

static void
cfg_viz_print_edge(const LogExprNode *node_parent, const LogExprNode *node_child, FILE *file)
{
    gchar tail_name[32];
    gchar tail_props[32];
    gchar head_name[32];
    gchar head_props[32];

    cfg_viz_get_node_id(node_parent, tail_name, sizeof(tail_name));
    cfg_viz_get_node_props(node_parent, FALSE, tail_props, sizeof(tail_props));

    //To distinguish different junctions
    if(node_child->content == ENC_PIPE && node_child->layout == ENL_JUNCTION)
        junc_count++;

    cfg_viz_get_node_id(node_child, head_name, sizeof(head_name));
    cfg_viz_get_node_props(node_child, TRUE, head_props, sizeof(head_props));

    fprintf(file, "\t\"%s\" -> \"%s\" [%s %s color=%s];\n",
            tail_name, head_name, tail_props, head_props, color[count]);
}

static const char*
cfg_viz_node_get_shape(const gint content)
{
    switch(content)
    {
        case ENC_PIPE: return "triangle";
        case ENC_SOURCE: return "box";
        case ENC_DESTINATION: return "doublecircle";
        case ENC_FILTER: return "diamond";
        case ENC_PARSER: return "circle";
        case ENC_REWRITE: return "parallelogram";
        default: return "star";
    }
}

static LogExprNode *
cfg_viz_print_channel(LogExprNode *node, const int id, FILE *file)
{
    fprintf(file, "\t\tsubgraph cluster_%d_%d\n\t\t{\n\t\t\tlabel=\"channel\";\n",
            count, id);

    if(!node)
    {
        fprintf(file, "\t\t\tsecret_head_%d_%d [style=invis shape=point];\n", count, id);
        fprintf(file, "\t\t}\n");
        return NULL;
    }

    if(!node->next)
    {
        gchar name[32];
        cfg_viz_get_node_id(node, name, sizeof(name));
        fprintf(file, "\t\"%s\";\n", name);
    }

    while(node->next)
    {
        fprintf(file, "\t\t");
        cfg_viz_print_edge(node, node->next, file);

        node = node->next;
    }
    fprintf(file, "\t\t}\n");

    return node;
}

static void
cfg_viz_print_junction(const LogExprNode *fork, FILE *file)
{
    LogExprNode *node = fork->next->children;

    int i;
    for(i = 0; node; node = node->next)
    {
        LogExprNode *n_node = node->children;

        fprintf(file, "\n\tsubgraph cluster_%d__%d\n\t{\n\t\tlabel=\"junction\";\n", count, junc_count);
        fprintf(file, "\t\tsecret_head%d[style=invis shape=point];\n", junc_count);

        n_node = cfg_viz_print_channel(n_node, i++, file);

        fprintf(file, "\t}\n\n");
    }
}

static LogExprNode*
cfg_viz_skip_sources(LogExprNode *node)
{
    while(node->next->content == ENC_SOURCE)
    {
        if(node->next) node = node->next;
        else break;
    }

    return node;
}

//TODO: It's disgusting
static LogExprNode*
cfg_viz_merge_destinations(const LogExprNode *node, LogExprNode *n_node, FILE *file)
{
    int finish_flag = FALSE;
    do
    {
        if(n_node->content != ENC_DESTINATION)
            finish_flag = TRUE;

        cfg_viz_print_edge(node, n_node, file);

        if(n_node->next && !finish_flag)
            n_node = n_node->next;
        else
            break;

    } while(n_node && !finish_flag);

    return n_node;
}

/* Iterates through the tree in the given LogExprNode writing it's
 * node's to the output. It displays all the nodes according to their types.
*/
static void
cfg_viz_print_tree(LogExprNode *node, FILE *file)
{
    while(node->next)
    {
        switch(node->next->content)
        {
            case ENC_SOURCE:
                {
                    LogExprNode *last_src = cfg_viz_skip_sources(node);

                    //Not just print_edge, see: source, source, dest, dest
                    cfg_viz_merge_destinations(node, last_src->next, file);
                    break;
                }

            case ENC_DESTINATION:
                {
                    node = cfg_viz_merge_destinations(node, node->next, file);
                    continue;
                }
            case ENC_PIPE:
                if(node->next->layout == ENL_JUNCTION)
                {
                    cfg_viz_print_edge(node, node->next, file);
                    cfg_viz_print_junction(node, file);
                }
                break;
            default:
                cfg_viz_print_edge(node, node->next, file);
                break;
        }

        node = node->next;
    }
}

/* Each LogPipe is printed with a different color */
static void
cfg_viz_print_rules(const GlobalConfig *config, FILE *file)
{
    int i, length;

    length = config->tree.rules->len;
    for(i = 0; i < length; i++)
    {
        LogPipe *pipe = (LogPipe *)g_ptr_array_index(config->tree.rules, i);
        LogExprNode *head = pipe->pipe_next->expr_node->children;

        cfg_viz_print_tree(head, file);

        //To handle when we run out of colors
        if(++count == color_count) count = 0;
    }
}

/* It is necessary to print out all the nodes (source, filter, parser,
 * rewrite, destination) at first, because this way it can be seen if
 * a node is not part of any LogPipes.
 *
 * The id is needed to distinguish different types of nodes with the same
 * name, but in the graph the name text will be seen.
 */
static void
cfg_viz_print_props(gpointer key, gpointer value, gpointer user_data)
{
    LogExprNode *node = (LogExprNode *)value;
    FILE *file = (FILE *)user_data;
    gchar id[32];

    cfg_viz_get_node_id(node, id, sizeof(id));

    fprintf(file, "\t\"%s\" [label=\"%s\" shape=\"%s\"];\n",
            id, node->name, cfg_viz_node_get_shape(node->content));
}

static void
cfg_viz_print_node_objects(const GlobalConfig *config, FILE *file)
{
    g_hash_table_foreach(config->tree.objects, cfg_viz_print_props, file);
}

void
cfg_viz_init(const GlobalConfig *config, const gchar *file_name)
{
    FILE *file = fopen(file_name, "w+");

    if(!file)
    {
        printf("Could not open file\n");
        return;
    }

    fprintf(file, "digraph G\n{\n");
    fprintf(file, "\tcompound=true;\n");

    cfg_viz_print_node_objects(config, file);
    cfg_viz_print_rules(config, file);

    fprintf(file, "}");
    fclose(file);
}

