"""List pinned source dependencies; fetch only with explicit --fetch. No software installs."""
from pathlib import Path
import argparse,json,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--fetch',action='store_true');args=p.parse_args()
root=Path(__file__).resolve().parents[1]
for item in json.loads((root/'source-dependencies.json').read_text(encoding='utf-8')):
 target=(root/item['path']).resolve()
 if not target.is_relative_to(root):raise SystemExit('Dependency path escapes project')
 print(item['name'],item['commit'],item['path'])
 if not args.fetch:continue
 if target.exists():
  if not (target/'.git').exists():raise SystemExit('Refusing existing non-repository '+str(target))
  sha=subprocess.check_output(['git','-C',str(target),'rev-parse','HEAD'],text=True).strip()
  if sha!=item['commit']:raise SystemExit('Existing dependency revision differs: '+item['name'])
  patch=item.get('patch')
  if patch:subprocess.run(['git','-C',str(target),'apply','--reverse','--check',str(root/patch)],check=True)
  print('Already present; left unchanged');continue
 target.parent.mkdir(parents=True,exist_ok=True)
 subprocess.run(['git','clone','--no-checkout',item['url'],str(target)],check=True)
 subprocess.run(['git','-C',str(target),'checkout','--detach',item['commit']],check=True)
 if item.get('patch'):subprocess.run(['git','-C',str(target),'apply',str(root/item['patch'])],check=True)
print('libjbc and separately copyrighted runtime assets are NOT fetched. Read docs/BUILDING.md.')
