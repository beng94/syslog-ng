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
cfg_viz_get_node_name(LogExprNode *node, gchar* buf, size_t size)
{
    g_snprintf(buf, size, "%s", node->name);
}

static void
cfg_viz_get_node_id(LogExprNode *node, gchar* buf, size_t size)
{
    gchar name_buf[32];
    cfg_viz_get_node_name(node, name_buf, sizeof(name_buf));

    if(node->content == ENC_PIPE && node->layout == ENL_JUNCTION)
        g_snprintf(buf, size, "secret_head%d", junc_count);
    else
        g_snprintf(buf, size, "%d%s", node->content, name_buf);
}

static void
cfg_viz_get_node_props(LogExprNode *node, gboolean head, gchar* buf, size_t size)
{
    if(node->content == ENC_PIPE &&
       node->layout == ENL_JUNCTION)
    {
       if(head) g_snprintf(buf, size, "lhead=cluster_%d__%d", count, junc_count);
       else g_snprintf(buf, size, "ltail=cluster_%d__%d", count, junc_count);
    }
    else g_snprintf(buf, size, "\0'");
    //else leave buf empty
}

static void
cfg_viz_print_edge(LogExprNode *node_parent, LogExprNode *node_child, FILE *file)
{
    gchar tail_name[32];
    gchar tail_props[32];
    gchar head_name[32];
    gchar head_props[32];

    cfg_viz_get_node_id(node_parent, tail_name, sizeof(tail_name));
    cfg_viz_get_node_props(node_parent, FALSE, tail_props, sizeof(tail_props));

    if(node_parent->content == ENC_PIPE && node_parent->layout == ENL_JUNCTION &&
       node_child->content == ENC_PIPE && node_child->layout == ENL_JUNCTION)
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

//TODO: join is not used
static void
cfg_viz_print_junction(LogExprNode *fork, LogExprNode *join, FILE *file)
{
    LogExprNode *node = fork->next->children;

    int i;
    for(i = 0; node; node = node->next)
    {
        LogExprNode *n_node = node->children;

        fprintf(file, "\nsubgraph cluster_%d__%d\n{\n\tlabel=\"junction\";\n", count, junc_count);
        fprintf(file, "\tsecret_head%d[style=invis shape=point];\n", junc_count);

        n_node = cfg_viz_print_channel(n_node, i++, file);

        fprintf(file, "}\n\n");
    }
}

static LogExprNode*
cfg_viz_skip_sources(LogExprNode *node)
{
    while(node->next->content == ENC_SOURCE)
    {
        if(node->next)
            node = node->next;
        else break;
    }

    return node;
}

static LogExprNode*
cfg_viz_merge_destinations(LogExprNode *node, LogExprNode *n_node, FILE *file)
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

static void
cfg_viz_print_tree(LogExprNode *node, FILE *file)
{
    while(node)
    {
        /*if(node->children)
        {
            cfg_viz_print_edge(node, node->children, file);
            cfg_viz_print_tree(node->children, file);
        }*/

        if(node->next)
        {
            switch(node->next->content)
            {
                case ENC_SOURCE:
                    {
                        LogExprNode *last_src = cfg_viz_skip_sources(node);

                        //TODO: Do I really need this condition?
                        if(last_src->next->content == ENC_PIPE &&
                           last_src->next->layout == ENL_JUNCTION)
                        {
                            cfg_viz_print_edge(node, last_src->next, file);
                        }
                        else
                        {
                            cfg_viz_merge_destinations(node, last_src->next, file);
                        }
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
                        //junc_count++;
                        cfg_viz_print_edge(node, node->next, file);
                        cfg_viz_print_junction(node, node->next->next, file);

                        //cfg_viz_print_edge(node->next, node->next->next, file);
                        //node = node->next;
                    }
                    break;
                default:
                    cfg_viz_print_edge(node, node->next, file);
                    break;
            }
        }

        node = node->next;
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
cfg_viz_init(GlobalConfig *config, const gchar *file_name)
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
    //cfg_viz_print_pipes(config, file);
    cfg_viz_print_rules(config, file);

    fprintf(file, "}");
    fclose(file);
}
