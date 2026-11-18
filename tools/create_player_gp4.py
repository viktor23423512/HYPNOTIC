from pathlib import Path
import sys,subprocess,xml.etree.ElementTree as ET
root=Path.cwd();base=['eboot.bin','sce_sys/about/right.sprx','sce_sys/param.sfo','sce_sys/icon0.png'];subprocess.run([sys.argv[1],'-out','pkg.gp4','--content-id='+sys.argv[2],'--files',' '.join(base)],check=True)
tree=ET.parse('pkg.gp4');files=tree.getroot().find('files')
for folder in ['sce_module','assets']:
 for p in sorted((root/folder).rglob('*')):
  if p.is_file():name=p.relative_to(root).as_posix();ET.SubElement(files,'file',{'targ_path':name,'orig_path':name})
tree.write('pkg.gp4',encoding='utf-8',xml_declaration=True)
