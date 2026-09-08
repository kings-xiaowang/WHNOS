from PIL import Image
for n in ('early','final'):
    im = Image.open('/mnt/d/WHNos/output/'+n+'.pbm').convert('RGB')
    im = im.resize((im.width*3, im.height*3), Image.NEAREST)
    im.save('/mnt/d/WHNos/output/'+n+'.png')
    print(n, im.size)
