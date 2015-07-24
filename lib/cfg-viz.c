#include <cfg-viz.h>

static int count = 0;
static int color_count = 31;

const gchar* color[] =
{ "blue", "brown", "coral", "chartreuse", "cadetblue",
  "crimson", "cyan", "darkgreen", "darkorange", "deeppink",
  "gold", "firebrick", "darkslateblue","green", "greenyellow",
  "hotpink", "indigo", "lightcoral", "limegreen", "maroon",
  "navy", "orange", "orangered", "orchid", "peru", "red",
  "royalblue", "tan", "yellow", "violet", "yellowgreen" };

static void
cfg_viz_get_node_name(LogExprNode *node, gchar* buf, size_t size)
{
    //To handle filters
    if(node->layout == ENL_SINGLE && node->content == ENC_PIPE)
        g_snprintf(buf, size, "%s", node->parent->name);
    else
       g_snprintf(buf, size, "%s", node->name);
}

static void
cfg_viz_print_node_id(LogExprNode *node, gchar* buf, size_t size)
{
    gchar name_buf[32];
    cfg_viz_get_node_name(node, name_buf, sizeof(name_buf));

     g_snprintf(buf, size, "%d%s",
             node->content,
             name_buf);
}

static void
cfg_viz_print_edge(LogExprNode *node_parent, LogExprNode *node_child, FILE *file)
{
    gchar buf_parent[32];
    gchar buf_child[32];

    cfg_viz_print_node_id(node_parent, buf_parent, sizeof(buf_parent));
    cfg_viz_print_node_id(node_child, buf_child, sizeof(buf_child));

    fprintf(file, "\t\"%s\" -> \"%s\"[color=%s];\n",
            buf_parent,
            buf_child,
            color[count]);
}

static const char*
cfg_viz_node_get_shape(const gint content)
{
    switch(content)
    {
        case ENC_PIPE:
            return "triangle";
        case ENC_SOURCE:
            return "box";
        case ENC_DESTINATION:
            return "doublecircle";
        case ENC_FILTER:
            return "diamond";
        case ENC_PARSER:
            return "circle";
        case ENC_REWRITE:
            return "parallelogram";
        default:
            return "star";
    }
}

static void
cfg_viz_print_node_props(LogExprNode *node, FILE *file)
{
    gchar name_buf[32];
    gchar node_name[32];

    cfg_viz_print_node_id(node, name_buf, sizeof(name_buf));
    cfg_viz_get_node_name(node, node_name, sizeof(node_name));

    fprintf(file, "\t\"%s\" [label=\"%s\" shape=\"%s\"];\n",
                name_buf,
                node_name,
                cfg_viz_node_get_shape(node->content));
}

static LogExprNode *
cfg_viz_print_channel(LogExprNode *node, FILE *file)
{
    while(node->next)
    {
        cfg_viz_print_edge(node, node->next, file);

        node = node->next;
    }

    return node;
}

static void
cfg_viz_print_junction(LogExprNode *fork, LogExprNode *join, FILE *file)
{
    LogExprNode *node = fork->next->children;

    while(node)
    {
        LogExprNode *n_node = node->children;

        cfg_viz_print_edge(fork, n_node, file);
        n_node = cfg_viz_print_channel(n_node, file);
        cfg_viz_print_edge(n_node, join, file);

        node = node->next;
    }
}

static void
cfg_viz_print_tree(LogExprNode *node, FILE *file)
{
    if(node->children)
    {
            cfg_viz_print_edge(node, node->children, file);
            cfg_viz_print_tree(node->children, file);
    }

    if(node->next)
    {
        if(node->next->content == ENC_PIPE &&
           node->next->layout == ENL_JUNCTION)
        {
            cfg_viz_print_junction(node, node->next->next, file);
            cfg_viz_print_tree(node->next->next, file);
        }
        else
        {
            cfg_viz_print_edge(node, node->next, file);
            cfg_viz_print_tree(node->next, file);
        }
    }
}

static void
cfg_viz_print_props(gpointer key, gpointer value, gpointer user_data)
{
    LogExprNode *node = (LogExprNode *)value;
    FILE *file = (FILE *)user_data;

    cfg_viz_print_node_props(node, file);
}

void
cfg_viz_init(GlobalConfig *config)
{
    FILE *file = fopen("/home/bence/Desktop/cfg_out.dot", "w+");

    if(!file)
    {
        printf("Could not open file\n");
        return;
    }

    fprintf(file, "digraph G\n{\n");

    g_hash_table_foreach(config->tree.objects, cfg_viz_print_props, file);

    int i;
    for(i = 0; i < config->tree.initialized_pipes->len; i++)
    {
        LogPipe *pipe = (LogPipe *)g_ptr_array_index(config->tree.initialized_pipes, i);

        if(pipe->expr_node->content == ENC_SOURCE &&
           pipe->expr_node->layout == ENL_REFERENCE)
        {
            if(pipe->expr_node->parent->children != pipe->expr_node &&
               pipe->expr_node->parent->children->content == ENC_SOURCE &&
               pipe->expr_node->parent->children->layout == ENL_REFERENCE)
            {
                continue;
            }
            //cfg_viz_traverse_pipe(pipe, file);
            cfg_viz_print_tree(pipe->expr_node, file);

            count++;
            if(count == color_count)
                count = 0;
        }
    }

    fprintf(file, "}");
    fclose(file);
}
