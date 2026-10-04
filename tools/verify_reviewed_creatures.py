#!/usr/bin/env python3
"""Compare emitted creature geometry with the actual reviewer and prepare native texture bindings."""
import argparse
import copy
import csv
import hashlib
import json
from pathlib import Path
import re
import subprocess

import numpy as np
from PIL import Image
import yaml

REPO = Path(__file__).resolve().parents[1]

# Native MM6 groups retained by the table but unused by current monster descriptors.
# Their restored unused art remains addressable without adding it to playback.
UNUSED_MM6_GROUPS = {
    'batsta': 'mm6_bat', 'b2tsta': 'mm6_bat', 'b3tsta': 'mm6_bat',
    'cdr1ata0': 'mm6_cdr1', 'cdr2ata0': 'mm6_cdr1', 'cdr3ata0': 'mm6_cdr1',
    'ske1dyf': 'mm6_ske', 'ske2dyf': 'mm6_ske', 'ske3dyf': 'mm6_ske',
}


def read(path):
    return json.loads(path.read_text())


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--recipe', type=Path, default=REPO/'assets_source/creatures/mm6/adaptations.json')
    parser.add_argument('--lossless', type=Path, required=True)
    parser.add_argument('--gpu', type=Path)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    args.report = args.report.resolve()
    args.lossless = args.lossless.resolve()
    recipe = read(args.recipe)
    payload, groups, manifests = [], {}, {}
    frame_count = step_count = 0
    maximum_float_error = 0
    for key, family in recipe['families'].items():
        bank = args.recipe.parent/key
        selected = read(bank/'selected.json')
        manifest = read(bank/selected['metadata']['manifest.json']['path'])
        package = 'mm6_gob' if key=='mm6_goblin' else key
        export = read(args.lossless/package/'manifest.json')
        manifests[package] = export
        assert export['adaptations_sha256']==sha(args.recipe)
        assert export['brightness_multiplier']==family['brightness_multiplier']
        assert export['animations_by_palette']==manifest['animations_by_palette']
        assert export['native_group_bindings']==manifest['native_group_bindings']
        assert set(manifest['frames'])==set(family['frames'])
        aliases={n for mapping in export.get('palette_frame_aliases',{}).values() for n in mapping.values()}
        assert set(export['frames'])==set(manifest['frames'])|aliases
        for mapping in export.get('palette_frame_aliases',{}).values():
            for native_name,alias in mapping.items():
                for field in ['crop_origin_px','draw_size_px']:
                    assert export['frames'][native_name][field]==export['frames'][alias][field]
        if args.gpu:
            gpu = read(args.gpu/package/'manifest.json')
            for field in ['frames','variants','logical_canvas','logical_pivot','brightness_multiplier',
                          'animations_by_palette','native_group_bindings','adaptations_sha256']:
                assert gpu[field]==export[field], (key,field)
        bounds = {}
        for index,page in enumerate(selected['pages']):
            alpha = Image.open(bank/page['base']['path']).getchannel('A').point(lambda x:255 if x>=128 else 0)
            for name,frame in manifest['frames'].items():
                if frame['page']!=index:continue
                x,y,w,h=frame['atlas_xywh'];bounds[name]=alpha.crop((x,y,x+w,y+h)).getbbox()
        manifest['review_restored_bounds']=bounds
        payload.append(dict(id=key,manifest=manifest,fixups=dict(frames=family['frames']),emitted={n:export['frames'][n] for n in manifest['frames']}))
        frame_count += len(manifest['frames'])
        for name,frame in export['frames'].items():
            # Mirrors and native scales are applied after this cache-derived crop/size.
            origin,extent=np.array(frame['crop_origin_px']),np.array(frame['draw_size_px'])
            float_origin,float_extent=origin.astype('f4'),extent.astype('f4')
            maximum_float_error=max(maximum_float_error,float(np.max(abs(origin-float_origin))),float(np.max(abs(extent-float_extent))))
        for palette, animations in manifest['animations_by_palette'].items():
            for action, animation in animations.items():
                if animation.get('no_image'):continue
                group=animation['native_group'].lower()
                if group=='null':continue
                native = manifest['native_group_bindings'].get(group)
                if native is None:
                    candidates = [g for descriptor in manifest['native_group_bindings'].values()
                                  for g in descriptor['actions'].values() if g['sprite_name'].lower()==group]
                    matching = [g for g in candidates if g['frames'][0]['palette_id']==int(palette)]
                    if not matching:
                        assert candidates and all(g==candidates[0] for g in candidates)
                        fixed_palette = str(candidates[0]['frames'][0]['palette_id'])
                        assert fixed_palette in export['variants'] and export['variants'][fixed_palette].get('exact_base_bypass')
                    native = (matching or candidates)[0]
                item=dict(package=package,palette=palette,animation=animation,action=action,native=native)
                if group in groups and groups[group][0]['package']!=package:
                    assert group=='m498x' and {package,groups[group][0]['package']}=={'mm6_dmn','mm6_wdm'}
                    continue
                groups.setdefault(group,[]).append(item)
                step_count += sum(len(v) for v in animation['views'].values())
    descriptor_path = REPO/'assets_dev/engine/data_tables/monster_descriptors.txt'
    descriptor_rows = list(csv.reader(descriptor_path.open(), delimiter='\t'))
    sprite_columns = [i for i, field in enumerate(descriptor_rows[1]) if field.startswith('Sprite')]
    active_groups = {row[i].lower() for row in descriptor_rows[2:] if row and row[0].isdigit()
                     for i in sprite_columns}
    assert not active_groups.intersection(UNUSED_MM6_GROUPS), 'Previously unused MM6 group now has a consumer'
    js = r'''
const fs=require('fs'),vm=require('vm');const code=fs.readFileSync(process.argv[1],'utf8');
function fn(n,next){return code.slice(code.indexOf('function '+n+'('),code.indexOf('function '+next+'('));}
const c={state:{},$:(id)=>({checked:true})};vm.createContext(c);
vm.runInContext(fn('actions','action')+fn('tier','frameFixup')+fn('frameFixup','previewOffset')+
 fn('previewOffset','restoredGeometry')+fn('restoredGeometry','imagePath')+fn('referenceScale','frameScale')+fn('frameScale','configureCanvas'),c);
let frames=0,steps=0,max=0;
for(const f of JSON.parse(fs.readFileSync(0,'utf8'))){c.state.manifest=f.manifest;c.state.fixups=f.fixups;
 for(const name of Object.keys(f.emitted)){const e=f.emitted[name],g=c.restoredGeometry(name);
  for(const [a,b] of [[g.x,e.crop_origin_px[0]],[g.y,e.crop_origin_px[1]],[g.width,e.draw_size_px[0]],[g.height,e.draw_size_px[1]]]){
   const err=Math.abs(a-b);max=Math.max(err,max);if(err>1e-9)throw Error(f.id+'/'+name+' export coordinate mismatch '+err);
  }frames++;
 }
 for(const [pal,animations]of Object.entries(f.manifest.animations_by_palette)){c.state.palette=pal;
  for(const animation of Object.values(animations))for(const sequence of Object.values(animation.views))for(const step of sequence){
   const g=c.restoredGeometry(step.frame),e=f.emitted[step.frame],s=c.frameScale(step),t=f.manifest.pixels_per_logical_pixel,p=f.manifest.logical_pivot;
   for(const corner of [[0,0],[1,0],[0,1],[1,1]])for(let axis=0;axis<2;axis++){
    const mirror=axis===0&&step.mirrored?-1:1;
    const expected=p[axis]+mirror*s*((axis?g.y:g.x)/t+corner[axis]*(axis?g.height:g.width)/t-p[axis]);
    const actual=p[axis]+mirror*s*(e.crop_origin_px[axis]/t+corner[axis]*e.draw_size_px[axis]/t-p[axis]);
    const err=Math.abs(expected-actual);max=Math.max(max,err);if(err>1e-9)throw Error('Emitted mirrored/scaled pose mismatch');
   }steps++;
  }
 }
}process.stdout.write(JSON.stringify({frames,steps,maximum_coordinate_error:max}));
'''
    viewer = REPO / 'tools/creatures/acceptance_viewer/app.js'
    assert sha(viewer) == recipe['validation']['viewer_sha256'], 'Reviewed viewer implementation changed'
    result=subprocess.run(['node','-e',js,str(viewer)],
                          input=json.dumps(payload),text=True,capture_output=True,check=True)
    parity=json.loads(result.stdout)
    assert parity['frames']==frame_count==recipe['frame_count']
    assert maximum_float_error<0.0002
    table_root=args.report.parent/'tables'
    table_records=[];found=set();bindings=0;changed_bindings=0
    unused_found=set();unused_bindings=0;unused_changed=0
    paths=[REPO/'assets_dev/engine/rendering/sprite_frame_data_common.yml']
    paths+=sorted((REPO/'assets_dev/engine/rendering/sprite_frames').rglob('*.yml'))
    for path in paths:
        original=path.read_text();parsed=yaml.load(original,Loader=yaml.CSafeLoader);desired=copy.deepcopy(parsed)
        replacements=[]
        for group in desired.get('sprites',[]):
            name=group['sprite_name'].lower()
            if name in UNUSED_MM6_GROUPS:
                assert name not in unused_found, ('Duplicate unused group',name)
                unused_found.add(name)
                package=UNUSED_MM6_GROUPS[name]
                export=manifests[package]
                for frame in group['frames']:
                    texture=frame['texture_name'].split('/')[-1].lower()
                    assert str(frame['palette_id']) in export['variants'], (name,'unused palette')
                    stored=[texture] if 'Image1' in frame.get('flags',[]) else [texture+str(v) for v in range(5)]
                    assert all(n in export['frames'] for n in stored), (name,'missing unused art')
                    expected='atlas:'+package+'/'+texture
                    replacements.append((frame['texture_name'],expected))
                    unused_bindings+=1
                    unused_changed+=frame['texture_name']!=expected
                    frame['texture_name']=expected
                continue
            if name not in groups:continue
            assert name not in found, ('Duplicate active group',name);found.add(name)
            items=groups[name]
            animation=items[0]['animation']
            assert len(group['frames'])==len(animation['views']['0']),name
            assert group.get('animation_length_raw',0)==animation.get('native_length_raw',0),name
            for index,frame in enumerate(group['frames']):
                candidates=[item for item in items if int(item['palette'])==frame['palette_id']]
                item=(candidates or items)[0];animation=item['animation'];step=animation['views']['0'][index]
                assert frame.get('frame_length_raw',0)*62.5==step['duration_ms'],(name,index,'duration')
                assert frame.get('flags',[])==item['native']['frames'][index]['flags'],(name,index,'flags')
                if 'native_flags' in step:
                    assert frame.get('flags',[])==step['native_flags'],(name,index,'step flags')
                assert abs(frame.get('scale',1)-step.get('native_scale',1))<1e-6,(name,index,'scale')
                image1='Image1' in frame.get('flags',[])
                if image1:
                    texture=step['frame']
                    assert all(v[index]['frame']==texture for v in animation['views'].values()),(name,index,'Image1')
                else:
                    assert step['frame'].endswith('0'),(name,step['frame'])
                    texture=step['frame'][:-1]
                    for view,sequence in animation['views'].items():
                        stored=8-int(view) if sequence[index]['mirrored'] else int(view)
                        assert sequence[index]['frame']==texture+str(stored),(name,index,view)
                alias_map=manifests[item['package']].get('palette_frame_aliases',{}).get(str(frame['palette_id']),{})
                if alias_map:
                    if image1:
                        texture=alias_map[step['frame']]
                    else:
                        texture=alias_map[step['frame']][:-1]
                        for view in range(5):
                            original_name=animation['views'][str(view)][index]['frame']
                            assert alias_map[original_name]==texture+str(view)
                expected='atlas:'+item['package']+'/'+texture
                replacements.append((frame['texture_name'],expected));bindings+=1
                changed_bindings+=frame['texture_name']!=expected
                frame['texture_name']=expected
        if not replacements:continue
        selected_groups={g['sprite_name'].lower() for g in parsed['sprites']
                         if g['sprite_name'].lower() in groups or g['sprite_name'].lower() in UNUSED_MM6_GROUPS}
        current=None;index=0;lines=[]
        for line in original.splitlines(keepends=True):
            match=re.search(r'sprite_name:\s*["\']?([^"\'\s]+)',line)
            if match:current=match[1].lower()
            match=re.search(r'(texture_name:\s*)(["\']?)([^"\'\s]+)(["\']?)',line)
            if match and current in selected_groups and match[3].lower()!='null':
                old,new=replacements[index];assert match[3]==old
                line=line[:match.start(3)]+new+line[match.end(3):];index+=1
            lines.append(line)
        text=''.join(lines);assert index==len(replacements)
        assert yaml.load(text,Loader=yaml.CSafeLoader)==desired,'Non-texture table mutation'
        destination=table_root/path.relative_to(REPO/'assets_dev/engine')
        destination.parent.mkdir(parents=True,exist_ok=True);destination.write_text(text)
        table_records.append(dict(path=str(path.relative_to(REPO)),prepared=str(destination.relative_to(REPO)),
                                  before_sha256=sha(path),after_sha256=sha(destination),changed=original!=text))
    assert found==set(groups),sorted(set(groups)-found)
    assert unused_found==set(UNUSED_MM6_GROUPS), sorted(set(UNUSED_MM6_GROUPS)-unused_found)
    report=dict(recipe_sha256=sha(args.recipe),families=len(recipe['families']),geometry=parity,
                maximum_float_storage_error_px=maximum_float_error,native_groups=len(found),
                native_step_bindings=bindings,changed_step_bindings=changed_bindings,
                unused_groups=len(unused_found),unused_step_bindings=unused_bindings,
                changed_unused_step_bindings=unused_changed,unused_groups_absent_from_descriptors=True,
                native_timing_scales_flags_unchanged=True,cooked_metadata_verified=bool(args.gpu),tables=table_records)
    args.report.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items() if k!='tables'}))


if __name__=='__main__':main()
