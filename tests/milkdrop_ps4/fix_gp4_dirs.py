"""Fill GP4 directory inventory for project-local assets omitted by SDK generator."""
from pathlib import Path,PurePosixPath
import sys,xml.etree.ElementTree as ET
p=Path(sys.argv[1]);tree=ET.parse(p);project=tree.getroot();root=project.find('rootdir')
assert root is not None
for file in project.findall('./files/file'):
    parent=PurePosixPath(file.attrib['targ_path']).parent
    node=root
    for part in parent.parts:
        assert part not in ('..','/')
        child=next((c for c in node.findall('dir') if c.get('targ_name')==part),None)
        if child is None:child=ET.SubElement(node,'dir',{'targ_name':part})
        node=child
tree.write(p,encoding='utf-8',xml_declaration=True)
print('GP4 directory inventory verified')
