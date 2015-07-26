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
    g_snprintf(buf, size, "%s", node->name);
}

static void
cfg_viz_get_node_id(LogExprNode *node, gchar* buf, size_t size)
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

    cfg_viz_get_node_id(node_parent, buf_parent, sizeof(buf_parent));
    cfg_viz_get_node_id(node_child, buf_child, sizeof(buf_child));

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

//TODO: Print it in a separete subgraph
static LogExprNode *
cfg_viz_print_channel(LogExprNode *node, int id, FILE *file)
{
    fprintf(file, "\t\tsubgraph cluster_%d_%d\n\t\t{\n\t\tlabel=\"channel\";\n",
            count, id);

    if(!node)
    {
        fprintf(file, "\t\tsecret_head_%d_%d [style=invis shape=point];\n", count, id);
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
cfg_viz_print_junction(LogExprNode *fork, LogExprNode *join, FILE *file)
{
    LogExprNode *node = fork->next->children;

    int i;
    for(i = 0; node; node = node->next)
    {
        LogExprNode *n_node = node->children;

        fprintf(file, "\nsubgraph cluster_%d\n{\n\tlabel=\"junction\";\n", count);
        fprintf(file, "\tsecret_head%d[style=invis shape=point];\n", count);

        n_node = cfg_viz_print_channel(n_node, i++, file);

        fprintf(file, "}\n\n");
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
        if(node->next->content == ENC_SOURCE)
        {
            LogExprNode *n_node = node;
            while(n_node->next->content == ENC_SOURCE)
            {
                n_node = n_node->next;
            }

            if(n_node->next->content == ENC_PIPE &&
               n_node->next->layout == ENL_JUNCTION)
            {
                gchar buf[32];
                cfg_viz_get_node_id(node, buf, sizeof(buf));
                fprintf(file, "\t\"%s\" -> secret_head%d[lhead=cluster_%d color=%s];\n",
                        buf, count, count, color[count]);

                cfg_viz_print_tree(node->next, file);
            }

            else if(n_node->next->content == ENC_DESTINATION)
            {
                n_node = n_node->next;
                while(n_node->content == ENC_DESTINATION)
                {
                    cfg_viz_print_edge(node, n_node, file);
                    n_node = n_node->next;

                    if(!n_node) break;
                }

                    cfg_viz_print_tree(node->next, file);
            }

            else
            {
                cfg_viz_print_edge(node, n_node->next, file);
                cfg_viz_print_tree(node->next, file);
            }
        }
        else if(node->next->content == ENC_PIPE &&
                node->next->layout == ENL_JUNCTION)
        {
            gchar buf[32];
            cfg_viz_get_node_id(node, buf, sizeof(buf));
            fprintf(file, "\t\"%s\" -> secret_head%d[lhead=cluster_%d color=%s];\n",
                    buf, count, count, color[count]);

            cfg_viz_print_junction(node, node->next->next, file);

            cfg_viz_get_node_id(node->next->next, buf, sizeof(buf));
            fprintf(file, "\t\tsecret_head%d -> \"%s\"[ltail=cluster_%d color=%s];\n",
                    count, buf, count, color[count]);

            cfg_viz_print_tree(node->next->next, file);
        }

        else if(node->next->content == ENC_DESTINATION)
        {
            LogExprNode *n_node = node->next;
            while(n_node->content == ENC_DESTINATION)
            {
                cfg_viz_print_edge(node, n_node, file);
                n_node = n_node->next;

                if(!n_node) break;
            }

            if(n_node)
                cfg_viz_print_tree(n_node, file);
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

    gchar id[32];
    gchar name[32];

    cfg_viz_get_node_id(node, id, sizeof(id));
    cfg_viz_get_node_name(node, name, sizeof(name));

    fprintf(file, "\t\"%s\" [label=\"%s\" shape=\"%s\"];\n",
                id,
                name,
                cfg_viz_node_get_shape(node->content));
}

static void
cfg_viz_print_node_objects(GlobalConfig *config, FILE *file)
{
    g_hash_table_foreach(config->tree.objects, cfg_viz_print_props, file);
}

static void
cfg_viz_print_rules(GlobalConfig *config, FILE *file)
{
    int i, length;

    length = config->tree.rules->len;
    for(i = 0; i < length; i++)
    {
        LogPipe *pipe = (LogPipe *)g_ptr_array_index(config->tree.rules, i);

        if(pipe->pipe_next->expr_node->children->layout == ENL_REFERENCE)
        {
            cfg_viz_print_tree(pipe->pipe_next->expr_node->children, file);

            if(++count == color_count) count = 0;
        }

    }

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
    fprintf(file, "\tcompound=true;\n");

    cfg_viz_print_node_objects(config, file);
    //cfg_viz_print_pipes(config, file);
    cfg_viz_print_rules(config, file);

    fprintf(file, "}");
    fclose(file);
}
