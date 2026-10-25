from pathlib import Path
# Original analytic Q12 reference: 127 fine steps per semitone, no SDK data.
values=[int(4096*2**(i/1524)+0.5) for i in range(1524)]
lines=['/* Original analytic equal-tempered Q12 reference; not an SDK pitch table. */']
lines += [', '.join(str(v) for v in values[i:i+12])+',' for i in range(0,len(values),12)]
Path('port/psx/spu_pitch.inc').write_text('\n'.join(lines)+'\n',encoding='utf-8')
