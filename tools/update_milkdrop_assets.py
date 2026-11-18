from pathlib import Path
import json
root=Path(__file__).resolve().parents[1];r=root/'tests/milkdrop_ps4';paths=set()
for folder in [root/'assets/milkdrop',r/'assets/misc',r/'assets/images']:
 for p in folder.rglob('*'):
  if p.is_file() and p.suffix.lower() in ('.milk','.png','.jpg','.jpeg','.bin'):paths.add(p if p.is_relative_to(root/'assets') else root/'assets'/p.parent.name/p.name)
rows=['#pragma once','struct MilkdropBundledAsset {const char *path;size_t bytes,offset;};','static const MilkdropBundledAsset milkdropBundledAssets[]={'];offset=0
for p in sorted(paths):
 size=p.stat().st_size;rows.append('{'+json.dumps('/app0/'+p.relative_to(root).as_posix())+','+str(size)+','+str(offset)+'},');offset+=size
assert offset<=8*1024*1024
rows+=['};','static constexpr size_t milkdropBundledBytes='+str(offset)+';'];(root/'graphics/milkdrop_asset_inventory.h').write_text('\n'.join(rows)+'\n');print('MilkDrop package cache inventory:',len(paths),'files,',offset,'bytes')
