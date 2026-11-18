"""Build-only leaf-dispatch overlay; pinned projectM sources stay unchanged."""
from pathlib import Path
OLD='#define invoke_arg(argnum, retval) \\\n    assert(ctx->args[argnum]);    \\\n    ctx->args[argnum]->func(ctx->args[argnum], retval)\n\n'
NEW='/* HYPNOTIC: preserve the exact pointer/value conventions of leaf nodes,\n * while avoiding indirect calls for the most frequent argument types.\n * Compound/stateful expressions keep the upstream evaluator and order. */\nstatic inline void hypnotic_eval_argument(prjm_eval_exptreenode_t* argument, PRJM_EVAL_F** result)\n{\n    assert(argument);assert(result);assert(*result);\n    if(argument->func == prjm_eval_func_var){assert(argument->var);*result=argument->var;}\n    else if(argument->func == prjm_eval_func_const){**result=argument->value;}\n    else argument->func(argument,result);\n}\n#define invoke_arg(argnum, retval) hypnotic_eval_argument(ctx->args[argnum], retval)\n\n'
def generate(source, destination):
    text=Path(source).read_text()
    if text.count(OLD)!=1:raise RuntimeError('Upstream evaluator changed; review leaf optimization')
    Path(destination).parent.mkdir(parents=True,exist_ok=True)
    Path(destination).write_text(text.replace(OLD,NEW,1))
    return Path(destination)
